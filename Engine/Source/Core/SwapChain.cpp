#include "SwapChain.h"
#include "Device.h"
#include "CommandQueue.h"
#include "ScaldUtil.h"

#include <cstdint>

using namespace Scald;

SwapChain::SwapChain(Device* device, HWND hWnd, uint32_t width, uint32_t height, bool bIs4xMsaaState)
    : m_device(device),
      m_width(width),
      m_height(height)
{
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = /*RenderCommon::BackBufferFormat*/DXGI_FORMAT_R10G10B10A2_UNORM;                               // Back buffer format
    // swapChainDesc.SampleDesc = bIs4xMsaaState ? DXGI_SAMPLE_DESC{4u, m_4xMsaaQuality - 1u} : DXGI_SAMPLE_DESC{1u, 0u}; // MSAA
    swapChainDesc.SampleDesc = DXGI_SAMPLE_DESC{1u, 0u};                                                                  // MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = RenderCommon::SwapChainFrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullScreenDesc = {};
    swapChainFullScreenDesc.RefreshRate.Numerator = 60u;
    swapChainFullScreenDesc.RefreshRate.Denominator = 1u;
    swapChainFullScreenDesc.Windowed = TRUE;

    const auto factory = device->GetDXGIFactory();
    const auto cmdQueue = device->GetCommandQueue();

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(cmdQueue->GetCommandQueue().Get(),  // Swap chain needs the queue so that it can force a flush on it.
        hWnd, &swapChainDesc, &swapChainFullScreenDesc, nullptr, &swapChain));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_dxgiSwapChain));
}

SwapChain::~SwapChain() = default;

void Scald::SwapChain::SetFullscreen(bool fullscreen)
{
    if (m_bIsFullscreen == fullscreen) return;
    m_bIsFullscreen = fullscreen;
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetRTV() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), 
        8 + m_currBackBufferIndex, // TODO : here is the big problem since RT views are not zero and first indices decsriptors in RTV heap
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetDSV() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
        3, // TODO : here is the big problem since DS view are not zero index decsriptor in DSV heap
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
}

void SwapChain::ResetRenderTargets()
{
    for (UINT i = 0; i < RenderCommon::SwapChainFrameCount; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencilBuffer.Reset();
}

void SwapChain::Resize(uint32_t width, uint32_t height)
{
    ResetRenderTargets();

    ThrowIfFailed(m_dxgiSwapChain->ResizeBuffers(RenderCommon::SwapChainFrameCount, width, height, RenderCommon::BackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    // Create/recreate render targets and RTVs.
    for (UINT i = 0; i < RenderCommon::SwapChainFrameCount; i++)
    {
        ThrowIfFailed(m_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_device->AllocateRTV(&m_rtvDescriptorSlots[i]));
        m_device->GetD3D12Device()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);

        std::wstring name = L"Backbuffer[" + std::to_wstring(i) + L"]";
        SCALD_NAME_D3D12_OBJECT(m_renderTargets[i], name.c_str());
    }

    // Create/recreate Deoth-Stencil and DSV.
    D3D12_RESOURCE_DESC depthStencilDesc = {};
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_width;
    depthStencilDesc.Height = m_height;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;  // 24 bits for depth, 8 bits for stencil

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    m_dxgiSwapChain->GetDesc1(&swapChainDesc);
    depthStencilDesc.SampleDesc = {swapChainDesc.SampleDesc.Count, swapChainDesc.SampleDesc.Quality};  // MSAA: same settings as back buffer
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = RenderCommon::DepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0u;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /* Once created and never changed (from CPU) */);
    ThrowIfFailed(m_device->GetD3D12Device()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_device->AllocateDSV(&m_dsvDescriptorSlot));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = RenderCommon::DepthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0u;

    m_device->GetD3D12Device()->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);
    SCALD_NAME_D3D12_OBJECT(m_depthStencilBuffer, L"DepthStencilBuffer");

    // TODO: Command objects probably
    // There must be depth stencil buffer transition from its initial state to be used as a depth buffer.
    // It is made outside this function because it is not a responsibility of the swap chain to manage resource states.
    // But I might be wrong, so it is temporarily like that.
}

void SwapChain::Present()
{
    const uint32_t syncInterval = m_bIsVSyncEnabled ? 1u : 0u;
    const uint32_t presentFlags = m_bIsTearingSupported && !m_bIsVSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0u;
    // Present the frame.
    ThrowIfFailed(m_dxgiSwapChain->Present(syncInterval, presentFlags));
    m_currBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();
}

ID3D12Resource* Scald::SwapChain::GetBackBuffer() const
{
    return m_renderTargets[m_currBackBufferIndex].Get();
}

ID3D12Resource* SwapChain::GetDepthStencilBuffer() const
{
    return m_depthStencilBuffer.Get();
}