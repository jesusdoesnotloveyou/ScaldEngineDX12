#include "stdafx.h"
#include "D3D12Sample.h"
#include "CommandQueue.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

using namespace Microsoft::WRL;

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n - 1);
    }
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
};

D3D12Sample::D3D12Sample(UINT width, UINT height, const std::wstring& name, const std::wstring& className)
    :
    m_width(width),
    m_height(height),
    m_useWarpDevice(false),
    m_rtvDescriptorSize(0u),
    m_dsvDescriptorSize(0u),
    m_cbvSrvUavDescriptorSize(0u),
    m_currBackBuffer(0),
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

//static ExampleDescriptorHeapAllocator srvHeapAlloc;

int D3D12Sample::Run()
{
    //srvHeapAlloc.Create(m_device.Get(), m_srvHeap.Get());

    //// Setup Platform/Renderer backends
    //ImGui_ImplDX12_InitInfo init_info = {};
    //init_info.Device = m_device.Get();
    //init_info.CommandQueue = m_commandQueue->GetCommandQueue().Get();
    //init_info.NumFramesInFlight = 2u;
    //init_info.RTVFormat = BackBufferFormat;
    //init_info.DSVFormat = DepthStencilFormat;

    //init_info.SrvDescriptorHeap = m_srvHeap.Get();
    //init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return srvHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
    //init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return srvHeapAlloc.Free(cpu_handle, gpu_handle); };
    //ImGui_ImplDX12_Init(&init_info);

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

                // Start the Dear ImGui frame
                //ImGui_ImplDX12_NewFrame();
                //ImGui_ImplWin32_NewFrame();
                //ImGui::NewFrame();
                //ImGui::ShowDemoWindow();
                //
                //// Rendering
                //ImGui::Render();
                
                OnUpdate(m_timer);
                OnRender(m_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    /*ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();*/

    OnDestroy();
    //srvHeapAlloc.Destroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

void D3D12Sample::OnUpdate(const ScaldTimer& st)
{
#if defined(DEBUG) || defined(_DEBUG)
    static float timeStep = 0.0f;

    // Print GPU Memory usage info every 1 sec
    if (timeStep > 1.0f)
    {
        timeStep = 0.0f;
        // To check how much memory app is using from two pools: DXGI_MEMORY_SEGMENT_GROUP_LOCAL (L1) and DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL (L0)
        DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
        UINT nodeIndex = 0u; // Single-GPU
        if (SUCCEEDED(m_hardwareAdapter->QueryVideoMemoryInfo(nodeIndex, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo)))
        {
            std::wstring text = L"***VideoMemoryInfo***";
            text += L"\n\tBudget: " + std::to_wstring(BYTE_TO_MB(videoMemoryInfo.Budget));
            text += L"\n\tCurrentUsage: " + std::to_wstring(BYTE_TO_MB(videoMemoryInfo.CurrentUsage));
            text += L"\n\tAvailableForReservation: " + std::to_wstring(BYTE_TO_MB(videoMemoryInfo.AvailableForReservation));
            text += L"\n\tCurrentReservation: " + std::to_wstring(BYTE_TO_MB(videoMemoryInfo.CurrentReservation));
            text += L"\n";
            OutputDebugString(text.c_str());
        }
        timeStep += st.DeltaTime();
    }
#endif
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
#if defined(DEBUG) || defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    CreateDebugLayer();
#endif
    
    CreateDevice();

#if defined(DEBUG) || defined(_DEBUG)
    LogAdapters();
#endif

    CreateCommandObjectsAndInternalFence();
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
            UINT adapterIndex = 0u;
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

        ComPtr<ID3D12Debug1> debugController1;
        ThrowIfFailed(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)));
        debugController1->SetEnableGPUBasedValidation(true);

        // Enable additional debug layers.
        m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
}

VOID D3D12Sample::CreateDevice()
{
    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // use UMA video adapter if there is no dedicated
    //[[unlikely]] // C++20
    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(m_factory.Get(), &hardwareAdapter);
     
        ThrowIfFailed(hardwareAdapter.As(&m_hardwareAdapter));
        
        ThrowIfFailed(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }

    SCALD_NAME_D3D12_OBJECT(m_device, L"Graphics Device");

#if defined(DEBUG) || defined(_DEBUG)
    CheckFeatureSupport();
#endif
}

void D3D12Sample::CheckFeatureSupport()
{
    D3D12_FEATURE_DATA_ARCHITECTURE architectureInfo = {};
    if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &architectureInfo, sizeof(architectureInfo))))
    {
        UMA = architectureInfo.UMA;

        std::wstring text = L"***D3D12_FEATURE_ARCHITECTURE***";
        text += L"\n\tNodeIndex: " + std::to_wstring(architectureInfo.NodeIndex);
        text += L"\n\tTileBasedRenderer " + std::to_wstring(architectureInfo.TileBasedRenderer);
        text += L"\n\tUMA " + std::to_wstring(architectureInfo.UMA);
        text += L"\n\tCacheCoherentUMA " + std::to_wstring(architectureInfo.CacheCoherentUMA);
        text += L"\n";
        OutputDebugString(text.c_str());
    }
}

VOID D3D12Sample::CreateCommandObjectsAndInternalFence()
{
    // If we have multiple command queues, we can write a resource only from one queue at the same time.
    // Before it can be accessed by another queue, it must transition to read or common state.
    // In a read state resource can be read from multiple command queues simultaneously, including across processes, based on its read state.
    m_commandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);

    m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
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
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = BackBufferFormat; // Back buffer format
    swapChainDesc.SampleDesc = m_is4xMsaaState ? DXGI_SAMPLE_DESC{ 4u, m_4xMsaaQuality - 1u } : DXGI_SAMPLE_DESC{ 1u, 0u }; // MSAA
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
        m_commandQueue->GetCommandQueue().Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32App::GetHwnd(),
        &swapChainDesc,
        &swapChainFullScreenDesc,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(m_factory->MakeWindowAssociation(Win32App::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
}

VOID D3D12Sample::Reset()
{
    assert(m_device);
    assert(m_swapChain);
    assert(m_commandQueue);
    assert(m_commandAllocator);

    // Before making any changes
    m_commandQueue->Flush();

    auto commandList = m_commandQueue->GetCommandList(m_commandAllocator.Get());

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

    m_currBackBuffer = 0u;

    // Create/recreate frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < SwapChainFrameCount; i++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, m_rtvDescriptorSize);

            std::wstring name = L"Backbuffer[" + std::to_wstring(i) + L"]";
            m_renderTargets[i]->SetName(name.c_str());
        }

        // Create the depth/stencil view.
        D3D12_RESOURCE_DESC depthStencilDesc = {};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = m_width;
        depthStencilDesc.Height = m_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // 24 bits for depth, 8 bits for stencil
        depthStencilDesc.SampleDesc = m_is4xMsaaState ? DXGI_SAMPLE_DESC{ 4u, m_4xMsaaQuality - 1u } : DXGI_SAMPLE_DESC{ 1u, 0u }; // MSAA: same settings as back buffer

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
        m_depthStencilBuffer->SetName(L"DepthStencilBuffer");
    }
    
    // Transition the resource from its initial state to be used as a depth buffer.
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Execute the resize commands. 
    m_commandQueue->ExecuteCommandList(commandList);

    // Wait until resize is complete.
    m_commandQueue->Flush();

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

VOID D3D12Sample::Present()
{
    /*UINT syncInterval = m_VSync ? 1 : 0;
    UINT presentFlags = m_IsTearingSupported && !m_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;*/
    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1u, 0u));

    m_currBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
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

void D3D12Sample::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while (m_factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());

        adapterList.push_back(adapter);

        ++i;
    }

    for (size_t i = 0; i < adapterList.size(); ++i)
    {
        LogAdapterOutputs(adapterList[i]);
        SAFE_RELEASE(adapterList[i]);
    }
}

void D3D12Sample::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());

        LogOutputDisplayModes(output, BackBufferFormat);

        SAFE_RELEASE(output);

        ++i;
    }
}

void D3D12Sample::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;

    // Call with nullptr to get list count.
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for (auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";

        ::OutputDebugString(text.c_str());
    }
}