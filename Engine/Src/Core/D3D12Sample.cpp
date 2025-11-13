#include "stdafx.h"
#include "D3D12Sample.h"

using namespace Microsoft::WRL;

D3D12Sample::D3D12Sample(UINT width, UINT height, const std::wstring& name, const std::wstring& className)
    :
    m_width(width),
    m_height(height),
    m_useWarpDevice(false),
    m_rtvDescriptorSize(0u),
    m_dsvDescriptorSize(0u),
    m_cbvSrvUavDescriptorSize(0u),
    m_frameIndex(0),
    m_title(name),
    m_class(className)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

D3D12Sample::~D3D12Sample()
{
}

int D3D12Sample::Run()
{
    // Main sample loop.
    MSG msg = { 0 };

    m_timer.Reset();

    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0u, 0u, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            m_timer.Tick();

            if (!m_appPaused)
            {
                CalculateFrameStats();
                OnUpdate(m_timer);
                OnRender(m_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

bool D3D12Sample::Get4xMsaaState() const
{
    return m_is4xMsaaState;
}

void D3D12Sample::Set4xMsaaState(bool value)
{
    if (m_is4xMsaaState != value)
    {
        m_is4xMsaaState = value;
        CreateSwapChain();
        OnResize();
    }
}

void D3D12Sample::LoadPipeline()
{
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    CreateDebugLayer();
#endif

    CreateDevice();
    CreateCommandObjects();
    CreateFence();
    CreateRtvAndDsvDescriptorHeaps();
    CreateSwapChain();

    Reset();
}

VOID D3D12Sample::Pause()
{
    m_appPaused = true;
    m_timer.Stop();
}

VOID D3D12Sample::UnPause()
{
    m_appPaused = false;
    m_timer.Start();
}

void D3D12Sample::Resize()
{
    m_appPaused = true;
    m_resizing = true;
    m_timer.Stop();
}

void D3D12Sample::OnResize()
{
    m_appPaused = false;
    m_resizing = false;
    m_timer.Start();
    Reset();
}

VOID D3D12Sample::Reset()
{
    assert(m_device);
    assert(m_swapChain);

    // Before making any changes
    //FlushCommandQueue();

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

    for (UINT i = 0; i < SwapChainFrameCount; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencilBuffer.Reset();

    // Resize the swap chain
    ThrowIfFailed(m_swapChain->ResizeBuffers(
        SwapChainFrameCount,
        m_width,
        m_height,
        BackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    m_frameIndex = 0u;

    // Create/recreate frame resources.
    {
        // Create Render Targets
        // Start of the rtv heap
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < SwapChainFrameCount; i++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, m_rtvDescriptorSize);

            // Create command allocator for every back buffer
            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        }

        // Create the depth/stencil view.
        D3D12_RESOURCE_DESC depthStencilDesc = {};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = m_width;
        depthStencilDesc.Height = m_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; //
        // MSAA, same settings as back buffer
        depthStencilDesc.SampleDesc.Count = m_is4xMsaaState ? 4u : 1u;
        depthStencilDesc.SampleDesc.Quality = m_is4xMsaaState ? (m_4xMsaaQuality - 1u) : 0u;

        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear = {};
        optClear.Format = DepthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0u;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /* Once created and never changed (from CPU) */),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DepthStencilFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0u;

        m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);
    }

    // Transition the resource from its initial state to be used as a depth buffer.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Execute the resize commands. 
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Wait until resize is complete.
    //FlushCommandQueue();

    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<FLOAT>(m_width);
    m_viewport.Height = static_cast<FLOAT>(m_height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = 0L;
    m_scissorRect.top = 0L;
    m_scissorRect.right = static_cast<LONG>(m_width);
    m_scissorRect.bottom = static_cast<LONG>(m_height);

}

void D3D12Sample::CalculateFrameStats()
{
    // Code computes the average frames per second, 
    // and also the average time it takes to render one frame. 
    // These stats are appended to the window caption bar.
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // Compute averages over one second period.
    if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt; // fps = frameCnt / 1

        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);
        std::wstring frameStatsWindowText = L" fps: " + fpsStr + L" mspf: " + mspfStr;

        SetCustomWindowText(frameStatsWindowText.c_str());
        // Reset for next average.
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

// Helper function for resolving the full path of assets.
std::wstring D3D12Sample::GetAssetFullPath(LPCWSTR assetName) const
{
    return m_assetsPath + assetName;
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void D3D12Sample::GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
                ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

// Helper function for setting the window's title text.
void D3D12Sample::SetCustomWindowText(LPCWSTR text) const
{
    std::wstring windowText = m_title + L": " + text;
    SetWindowText(Win32App::GetHwnd(), windowText.c_str());
}

VOID D3D12Sample::CreateDebugLayer()
{
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        // Enable additional debug layers.
        m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
}

VOID D3D12Sample::CreateDevice()
{
    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        GetHardwareAdapter(m_factory.Get(), &m_hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }
}

VOID D3D12Sample::CreateCommandObjects()
{
    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    // the most often used are direct, compute and copy
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask;
    // If we have multiple command queues, we can write a resource only from one queue at the same time.
    // Before it can be accessed by another queue, it must transition to read or common state.
    // In a read state resource can be read from multiple command queues simultaneously, including across processes, based on its read state.
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[m_frameIndex])));

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(
        0u /*Single GPU*/,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get() /*Must match the command list type*/,
        nullptr,
        IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());
}

VOID D3D12Sample::CreateFence()
{
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        m_fenceValues[m_frameIndex]++;
        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForGPU();
    }
}

VOID D3D12Sample::CreateRtvAndDsvDescriptorHeaps()
{
    // Create descriptor heaps.
    // Descriptor heap has to be created for every GPU resource

    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = SwapChainFrameCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1u;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

VOID D3D12Sample::CreateSwapChain()
{
    // Describe and create the swap chain.
    //DXGI_SWAP_CHAIN_DESC sd;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = BackBufferFormat; // Back buffer format
    swapChainDesc.SampleDesc.Count = m_is4xMsaaState ? 4u : 1u; // MSAA
    swapChainDesc.SampleDesc.Quality = m_is4xMsaaState ? (m_4xMsaaQuality - 1u) : 0u; // MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SwapChainFrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullScreenDesc = {};
    swapChainFullScreenDesc.RefreshRate.Numerator = 60u;
    swapChainFullScreenDesc.RefreshRate.Denominator = 1u;
    swapChainFullScreenDesc.Windowed = TRUE;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32App::GetHwnd(),
        &swapChainDesc,
        &swapChainFullScreenDesc,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(m_factory->MakeWindowAssociation(Win32App::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

VOID D3D12Sample::WaitForGPU()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    m_fenceValues[m_frameIndex]++;
}

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_
void D3D12Sample::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
            _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
        {
            m_useWarpDevice = true;
            m_title = m_title + L" (WARP)";
        }
    }
}