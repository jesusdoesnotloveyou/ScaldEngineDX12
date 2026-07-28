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

    SwapChain(Device* device, HWND hWnd, uint32_t width, uint32_t height, bool m_bIs4xMsaaState = false);

public:
    ~SwapChain();

public:
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
    ID3D12Resource* GetDepthStencilBuffer() const;
    uint32_t GetBackBufferIndex() const { return m_currBackBufferIndex; }
    ComPtr<IDXGISwapChain3> GetSwapChain() const { return m_dxgiSwapChain; }
    
    // Get main render target's descriptors from swapChain
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;

private:
    void ResetRenderTargets();

private:
    Device* m_device;

    ComPtr<IDXGISwapChain3> m_dxgiSwapChain;

    // Backbuffers
    ComPtr<ID3D12Resource> m_renderTargets[RenderCommon::SwapChainFrameCount];
    uint32_t m_rtvDescriptorSlots[RenderCommon::SwapChainFrameCount];
    uint32_t m_dsvDescriptorSlot;
    uint32_t m_currBackBufferIndex = 0u;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    // Current size of swap chain
    uint32_t m_width;
    uint32_t m_height;

    bool m_bIsVSyncEnabled = true;
    bool m_bIsTearingSupported = false;
    bool m_bIsFullscreen = false;
};
}  // namespace Blainn