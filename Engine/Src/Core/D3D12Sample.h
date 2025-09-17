#pragma once

#include "DXHelper.h"
#include "Win32App.h"
#include "ScaldTimer.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12Sample
{
public:
    D3D12Sample(UINT width, UINT height, std::wstring name, std::wstring className);
    virtual ~D3D12Sample();

public:

    int Run();

    virtual void OnInit() = 0;
    virtual void OnUpdate(const ScaldTimer& st) = 0;
    virtual void OnRender(const ScaldTimer& st) = 0;
    virtual void OnDestroy() = 0;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

    virtual void LoadPipeline();

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
    __forceinline UINT GetWidth() const { return m_width; }
    __forceinline UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    const WCHAR* GetWindowClass() const { return m_class.c_str(); }

    void SetWidth(WORD newWidth) { m_width = newWidth; }
    void SetHeight(WORD newHeigth) { m_height = newHeigth; }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
    std::wstring GetAssetFullPath(LPCWSTR assetName);

    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false);

    void SetCustomWindowText(LPCWSTR text);

    VOID CreateDebugLayer();
    VOID CreateDevice();
    VOID CreateCommandObjects();
    VOID CreateFence();
    VOID CreateRtvAndDsvDescriptorHeaps();
    VOID CreateSwapChain();

    // Wait for pending GPU work to complete.
    VOID WaitForGPU();

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

    static const UINT SwapChainFrameCount = 2;
    static const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // Pipeline objects.
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;

    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[SwapChainFrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<ID3D12Resource> m_renderTargets[SwapChainFrameCount];
    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    // Synchronization objects.
    UINT m_frameIndex = 0u; // keep track of front and back buffers (see SwapChainFrameCount)
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValues[SwapChainFrameCount];

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

private:
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
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV()
    {
        return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }
};