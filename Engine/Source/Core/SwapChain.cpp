#include "SwapChain.h"

#include <cstdint>

using namespace Scald;

SwapChain::SwapChain(HWND hWnd, DXGI_FORMAT backBufferFormat)
{

}

SwapChain::~SwapChain() = default;

void SwapChain::ResetRenderTargets()
{

}

void SwapChain::Present()
{
    const uint32_t syncInterval = m_bIsVSyncEnabled ? 1u : 0u;
    const uint32_t presentFlags = m_bIsTearingSupported && !m_bIsVSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0u;

    // Present the frame.
    ThrowIfFailed(m_dxgiSwapChain->Present(syncInterval, presentFlags));

    m_currBackBuffer = m_dxgiSwapChain->GetCurrentBackBufferIndex();
}