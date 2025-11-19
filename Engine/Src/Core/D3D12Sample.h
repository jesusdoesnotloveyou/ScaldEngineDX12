#pragma once

#include "Common/DXHelper.h"
#include "Common/ScaldTimer.h"
#include "Win32App.h"

// target_link_libraries(${PROJECT_NAME} PRIVATE DirectXTK d3dcompiler dxguid dxgi d3d11 assimp)
//#pragma comment(lib, "d3dcompiler.lib")
//#pragma comment(lib, "d3d12.lib")
//#pragma comment(lib, "dxgi.lib")

class CommandQueue;

class D3D12Sample
{
public:
    D3D12Sample(UINT width, UINT height, const std::wstring& name, const std::wstring& className);
    virtual ~D3D12Sample();

public:

    int Run();

    VVOID OnInit() = 0;
    VVOID OnUpdate(const ScaldTimer& st) = 0;
    VVOID OnRender(const ScaldTimer& st) = 0;
    VVOID OnDestroy() = 0;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

    VVOID LoadPipeline();

    // Convenience overrides for handling mouse input.
    VVOID OnMouseDown(WPARAM btnState, int x, int y) {}
    VVOID OnMouseUp(WPARAM btnState, int x, int y) {}
    VVOID OnMouseMove(WPARAM btnState, int x, int y) {}
    // Samples override the event handlers to handle specific messages.
    VVOID OnKeyDown(UINT8 /*key*/) {}
    VVOID OnKeyUp(UINT8 /*key*/) {}

    VVOID Pause();
    VVOID UnPause();
    VVOID Resize();
    VVOID OnResize();
    VVOID Reset();

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

    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = true);

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

    
    void CheckFeatureSupport();
    void SetCustomWindowText(LPCWSTR text) const;

    VOID CreateDebugLayer();
    VOID CreateDevice();
    VOID CreateCommandObjectsAndInternalFence();
    VOID CreateSwapChain();
    VVOID CreateRtvAndDsvDescriptorHeaps();

    VOID Present();

protected:

    UINT m_dxgiFactoryFlags = 0u;

    // Viewport dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Adapter info.
    bool m_useWarpDevice;

    bool m_appPaused = false;        // is the application paused ?
    bool m_minimized = false;        // is the application minimized ?
    bool m_maximized = false;        // is the application maximized ?
    bool m_resizing = false;         // are the resize bars being dragged ?
    bool m_fullscreenState = false;  // fullscreen enabled
    bool m_isWireframe = false;      // Fill mode
    bool m_is4xMsaaState = false;
    UINT m_4xMsaaQuality = 0u;

    UINT m_rtvDescriptorSize;       // see m_rtvHeap
    UINT m_dsvDescriptorSize;       // see m_dsvHeap
    UINT m_cbvSrvUavDescriptorSize; // see m_cbvHeap

    XMFLOAT2 m_lastMousePos = { 0.0f, 0.0f };

    static const UINT SwapChainFrameCount = 2u;
    static const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // Pipeline objects.
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device2> m_device;

    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    std::shared_ptr<CommandQueue> m_commandQueue = nullptr;
    // Temporary allocator that is needed only for initialization stage (but could be used for smth else)
    ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;

    ComPtr<ID3D12Resource> m_renderTargets[SwapChainFrameCount];
    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    // Synchronization objects.
    UINT m_currBackBuffer = 0u;
    
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

private:
    BOOL UMA = FALSE;

    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
    // Window class.
    std::wstring m_class;

    ScaldTimer m_timer;

protected:
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV()
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currBackBuffer, m_rtvDescriptorSize);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV()
    {
        return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }
};