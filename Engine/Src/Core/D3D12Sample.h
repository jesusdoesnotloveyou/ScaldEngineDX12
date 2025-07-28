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

    int Run();

    virtual void OnInit() = 0;
    virtual void OnUpdate(const ScaldTimer& st) = 0;
    virtual void OnRender(const ScaldTimer& st) = 0;
    virtual void OnDestroy() = 0;

    virtual void Resize();
    virtual void OnResize();
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

private:
    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
    // Window class.
    std::wstring m_class;

    ScaldTimer mTimer;
};

///////////////////////////////////// Luna's d3dUtil.h/.cpp

static ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into out default buffer, we need to create an intermediate upload heap
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;                    // For buffers the size of the date we are copying in bytes.
    subResourceData.SlicePitch = subResourceData.RowPitch;  // For buffers the size of the date we are copying in bytes.

    // Schedule to copy the data to the default buffer resource. At a high level, the helper function UpdateSubresources will copy the CPU memory
    // into the intermediate upload heap. Then, using ID3D12CommandList::CopySubresourceRegion, the intermediate upload heap data will be copied to mBuffer.
    cmdList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0u, 1u, &subResourceData);
    cmdList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    // Note: uploadBuffer has to be kept alive after the above function calls because the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.
    return defaultBuffer;
}