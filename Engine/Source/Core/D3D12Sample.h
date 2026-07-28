#pragma once

#include "DXHelper.h"
#include "Common/ScaldTimer.h"
#include "Common/KeyboardDevice.h"
#include "Common/MousePad.h"

#include <memory>

namespace Scald
{
class CommandQueue;
class SwapChain;
class Device;

class D3D12Sample
{
public:
    D3D12Sample(UINT width, UINT height, const std::wstring& name, const std::wstring& className);
    virtual ~D3D12Sample();

public:
    int Run();

    VVOID OnInit() = 0 { LoadPipeline(); }

    VVOID OnUpdate(const ScaldTimer& st) = 0;
    VVOID OnRender(const ScaldTimer& st) = 0;
    VVOID OnDestroy() = 0;

    bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);

    // Load the rendering pipeline dependencies.
    void LoadPipeline();
    void CreateGraphicsContext();

    // Convenience overrides for handling mouse input.
    MousePad* GetMouse();
    KeyboardDevice* GetKeyboard();

    // Samples override the event handlers to handle specific messages.
    VOID OnKeyDown(UINT8 /*key*/) {}
    VOID OnKeyUp(UINT8 /*key*/) {}

    VOID Pause();
    VOID UnPause();
    VOID SetResizing(bool bIsResizing = false);
    VVOID OnResize();
    VOID Minimize();
    VOID Maximize();
    VOID RestoreSize(bool bIsMinimized);

    bool IsDeviceValid() const;
    FORCEINLINE bool IsResizing() const { return m_resizing; }
    // Timer stuff
    VOID CalculateFrameStats();
    // Accessors.
    FORCEINLINE UINT GetWidth() const { return m_width; }
    FORCEINLINE UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    const WCHAR* GetWindowClass() const { return m_class.c_str(); }

    void SetWidth(WORD newWidth) { m_width = newWidth; }
    void SetHeight(WORD newHeigth) { m_height = newHeigth; }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
    std::wstring GetAssetFullPath(LPCWSTR assetName) const;

    void SetCustomWindowText(LPCWSTR text) const;

    VOID CreateCommandObjectsAndInternalFence();

    VOID Present();

protected:
    KeyboardDevice m_keyboard;
    MousePad m_mouse;

    UINT m_dxgiFactoryFlags = 0u;

    // Viewport dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Adapter info.
    bool m_useWarpDevice = false;

    bool m_appPaused = false;        // is the application paused ?
    bool m_minimized = false;        // is the application minimized ?
    bool m_maximized = false;        // is the application maximized ?
    bool m_resizing = false;         // are the resize bars being dragged ?
    bool m_fullscreenState = false;  // fullscreen enabled
    bool m_isWireframe = false;      // Fill mode
    bool m_is4xMsaaState = false;
    UINT m_4xMsaaQuality = 0u;

    std::shared_ptr<CommandQueue> m_commandQueue = nullptr;
    // Temporary allocator that is needed only for initialization stage (but could be used for smth else)
    ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;

    // Synchronization objects.
    UINT m_currBackBuffer = 0u;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    // Pipeline objects inside.
    std::unique_ptr<Device> m_device;
    std::unique_ptr<SwapChain> m_swapChain;

private:
    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
    // Window class.
    std::wstring m_class;

    ScaldTimer m_timer;

protected:
    // Get rest of them from heaps
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;
};
}  // namespace Scald