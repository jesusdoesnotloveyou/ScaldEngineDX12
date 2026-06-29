#pragma once

#pragma once

#include "DXHelper.h"

namespace Scald
{
using namespace Microsoft::WRL;

class SwapChain
{
public:
    SwapChain(HWND hWnd, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);
    virtual ~SwapChain();

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
    void Reset(uint32_t width, uint32_t height);
    void Present();

    ID3D12Resource* GetBackBuffer() const;
    uint32_t GetBackBufferIndex() const { return m_currBackBuffer; }
    DXGI_FORMAT GetBackBufferFormat() const { return m_backBufferFormat; }
    ComPtr<IDXGISwapChain3> GetSwapChain() const { return m_dxgiSwapChain; }

private:
    void ResetRenderTargets();

private:
    ComPtr<IDXGISwapChain3> m_dxgiSwapChain;

    ComPtr<ID3D12Resource> m_renderTargets[SwapChainFrameCount];
    uint32_t m_currBackBuffer = 0u;
    DXGI_FORMAT m_backBufferFormat;

    HWND m_hWnd;

    uint32_t m_width;
    uint32_t m_height;

    bool m_bIsVSyncEnabled;
    bool m_bIsTearingSupported;
    bool m_bIsFullscreen;
};
}  // namespace Blainn