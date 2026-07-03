#include "SwapChain.h"
#include "Device.h"
#include "CommandQueue.h"
#include "ScaldUtil.h"

#include <cstdint>

using namespace Scald;

namespace
{
// Renderer common settings
constexpr UINT SwapChainFrameCount = 2u;
constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}  // namespace

SwapChain::SwapChain(Device* device, HWND hWnd, uint32_t width, uint32_t height, bool bIs4xMsaaState, DXGI_FORMAT backBufferFormat)
    : m_device(device),
      m_width(width),
      m_height(height)
{
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = backBufferFormat;                                                                              // Back buffer format
    // swapChainDesc.SampleDesc = bIs4xMsaaState ? DXGI_SAMPLE_DESC{4u, m_4xMsaaQuality - 1u} : DXGI_SAMPLE_DESC{1u, 0u}; // MSAA
    swapChainDesc.SampleDesc = DXGI_SAMPLE_DESC{1u, 0u};                                                                  // MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SwapChainFrameCount;
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

void SwapChain::ResetRenderTargets()
{
    for (UINT i = 0; i < SwapChainFrameCount; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencilBuffer.Reset();
}

void SwapChain::Resize(uint32_t width, uint32_t height)
{
    ResetRenderTargets();

    ThrowIfFailed(m_dxgiSwapChain->ResizeBuffers(SwapChainFrameCount, width, height, m_backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    // Create/recreate render targets and RTVs.
    for (UINT i = 0; i < SwapChainFrameCount; i++)
    {
        ThrowIfFailed(m_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_device->AllocateRTV(&m_rtvDescriptorSlots[i]));
        m_device->GetD3D12Device()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);

        std::wstring name = L"Backbuffer[" + std::to_wstring(i) + L"]";
        m_renderTargets[i]->SetName(name.c_str());
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
    optClear.Format = DepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0u;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /* Once created and never changed (from CPU) */);
    ThrowIfFailed(m_device->GetD3D12Device()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_device->AllocateDSV(&m_dsvDescriptorSlot));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DepthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0u;

    m_device->GetD3D12Device()->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);
    m_depthStencilBuffer->SetName(L"DepthStencilBuffer");

    // Transition the resource from its initial state to be used as a depth buffer.
    // TODO: command lists, queues and allocators
    //ScaldUtil::TransitionResource(commandList.Get(), m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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