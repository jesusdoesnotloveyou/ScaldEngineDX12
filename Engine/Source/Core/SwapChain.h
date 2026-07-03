#pragma once

#pragma once

#include "DXHelper.h"

namespace Scald
{
using namespace Microsoft::WRL;
using namespace DirectX;

class Device;

class SwapChain final
{
    friend class Device;

    SwapChain(Device* device, HWND hWnd, uint32_t width, uint32_t height, bool m_bIs4xMsaaState, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);

public:
    ~SwapChain();

public:
    static const uint32_t SwapChainFrameCount = 2u;

    bool IsFullscreen() const { return m_bIsFullscreen; }
    void SetFullscreen(bool fullscreen);
    void ToggleFullscreen() { SetFullscreen(!m_bIsFullscreen); }

    void SetVSync(bool vSync) { m_bIsVSyncEnabled = vSync; }
    bool GetVSync() const { return m_bIsVSyncEnabled; }
    void ToggleVSync() { SetVSync(!m_bIsVSyncEnabled); }
    void SetVSyncEnabled(bool value) { SetVSync(value); }
    bool IsTearingSupported() const { return m_bIsTearingSupported; }

    // void WaitForSwapChain();
    void Resize(uint32_t width, uint32_t height);
    void Present();

    ID3D12Resource* GetBackBuffer() const;
    uint32_t GetBackBufferIndex() const { return m_currBackBufferIndex; }
    DXGI_FORMAT GetBackBufferFormat() const { return m_backBufferFormat; }
    ComPtr<IDXGISwapChain3> GetSwapChain() const { return m_dxgiSwapChain; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() { return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), m_currBackBufferIndex, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() { return m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); }

private:
    void ResetRenderTargets();

private:
    Device* m_device;

    ComPtr<IDXGISwapChain3> m_dxgiSwapChain;

    // Backbuffers
    ComPtr<ID3D12Resource> m_renderTargets[SwapChainFrameCount];
    uint32_t m_rtvDescriptorSlots[SwapChainFrameCount];
    uint32_t m_dsvDescriptorSlot;
    DXGI_FORMAT m_backBufferFormat;
    uint32_t m_currBackBufferIndex = 0u;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    // Current size of swap chain
    uint32_t m_width;
    uint32_t m_height;

    bool m_bIsVSyncEnabled;
    bool m_bIsTearingSupported;
    bool m_bIsFullscreen;
};
}  // namespace Blainn