#pragma once

#include "DXHelper.h"
#include "Win32App.h"
#include "ScaldTimer.h"
#include "ScaldCoreTypes.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12Sample
{
public:
    D3D12Sample(UINT width, UINT height, std::wstring name, std::wstring className);
    virtual ~D3D12Sample();

    int Run();

    virtual void OnInit() = 0;
    virtual void OnUpdate(const ScaldTimer& st) = 0;
    virtual void OnRender(const ScaldTimer& st) = 0;
    virtual void OnDestroy() = 0;

    virtual void CreateRtvAndDsvDescriptorHeaps() {}

    // Convenience overrides for handling mouse input.
    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}
    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(UINT8 /*key*/) {}
    virtual void OnKeyUp(UINT8 /*key*/) {}

    virtual VOID Pause();
    virtual VOID UnPause();
    virtual VOID Resize();
    virtual VOID OnResize();
    virtual VOID Reset();

    // Timer stuff
    virtual void CalculateFrameStats();

    // Accessors.
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    const WCHAR* GetWindowClass() const { return m_class.c_str(); }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
    std::wstring GetAssetFullPath(LPCWSTR assetName);

    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false);

    void SetCustomWindowText(LPCWSTR text);

    UINT m_dxgiFactoryFlags = 0u;

    // Viewport dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Adapter info.
    bool m_useWarpDevice;

    bool mAppPaused = false;        // is the application paused ?
    bool mMinimized = false;        // is the application minimized ?
    bool mMaximized = false;        // is the application maximized ?
    bool mResizing = false;         // are the resize bars being dragged ?
    bool mFullscreenState = false;  // fullscreen enabled

    XMFLOAT2 mLastMousePos = {0.0f, 0.0f};
    float mRadius = 5.0f;
    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;

private:
    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
    // Window class.
    std::wstring m_class;

    ScaldTimer mTimer;
};