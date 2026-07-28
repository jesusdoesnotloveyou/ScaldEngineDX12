#include "D3D12Sample.h"
#include "Win32App.h"

#include "ScaldUtil.h"
#include "CommandQueue.h"
#include "Device.h"
#include "SwapChain.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "Device.h"

using namespace Scald;
using namespace Microsoft::WRL;

namespace
{
    // Renderer common settings
    constexpr UINT SwapChainFrameCount = 2u;
    constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT HeapHandleIncrement;
    ImVector<int> FreeIndices;

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
    : m_width(width),
      m_height(height),
      m_useWarpDevice(false),
      m_currBackBuffer(0),
      m_title(name),
      m_class(className)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

D3D12Sample::~D3D12Sample() {}

static ExampleDescriptorHeapAllocator srvHeapAlloc;

int D3D12Sample::Run()
{
    auto srvHeap = m_device->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srvHeapAlloc.Create(m_device->GetD3D12Device().Get(), srvHeap);

    // Setup Platform/Renderer backends
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = m_device->GetD3D12Device().Get();
    init_info.CommandQueue = m_commandQueue->GetCommandQueue().Get();
    init_info.NumFramesInFlight = 3u;
    init_info.RTVFormat = BackBufferFormat;
    init_info.DSVFormat = DepthStencilFormat;

    init_info.SrvDescriptorHeap = srvHeap;
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    { return srvHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    { return srvHeapAlloc.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    // Main sample loop.
    MSG msg = {0};

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
                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                ImGui::ShowDemoWindow();
                // Rendering
                ImGui::Render();
                OnUpdate(m_timer);
                OnRender(m_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    OnDestroy();
    srvHeapAlloc.Destroy();

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
        UINT nodeIndex = 0u;  // Single-GPU
        if (SUCCEEDED(m_device->GetDXGIAdapter()->QueryVideoMemoryInfo(nodeIndex, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo)))
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
        //CreateSwapChain();
        OnResize();
    }
}

void D3D12Sample::LoadPipeline()
{
    CreateGraphicsContext();
    CreateCommandObjectsAndInternalFence();

    OnResize();
}

void D3D12Sample::CreateGraphicsContext()
{
#if defined(DEBUG) | defined(_DEBUG)
    Device::EnableDebugLayer();
#endif
    m_device = Device::Create();
    m_device->CreateCommandObjectsAndInternalFences();
    m_device->CreateDescriptorHeaps();

    m_swapChain = m_device->CreateSwapChain(Win32App::GetHwnd(), m_width, m_height, BackBufferFormat);
}

MousePad* D3D12Sample::GetMouse()
{
    return &m_mouse;
}

KeyboardDevice* D3D12Sample::GetKeyboard()
{
    return &m_keyboard;
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

void D3D12Sample::SetResizing(bool bIsResizing)
{
    m_resizing = bIsResizing;
}

void D3D12Sample::OnResize()
{
    assert(m_device);
    assert(m_swapChain);
    // To device
    assert(m_commandQueue);
    assert(m_commandAllocator);

    // Before making any changes
    m_commandQueue->Flush();
    // TODO: ?
    auto commandList = m_commandQueue->GetCommandList(m_commandAllocator.Get());

    m_swapChain->Resize(m_width, m_height);
    ScaldUtil::TransitionResource(commandList.Get(), m_swapChain->GetDepthStencilBuffer(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // Execute the resize commands.
    // TODO: ?
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

    m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
}

void D3D12Sample::Minimize()
{
    m_appPaused = true;
    m_minimized = true;
    m_maximized = false;
}

void D3D12Sample::Maximize()
{
    m_appPaused = false;
    m_minimized = false;
    m_maximized = true;
}

void D3D12Sample::RestoreSize(bool bIsMinimized)
{
    m_appPaused = false;
    if (bIsMinimized)
    {
        m_minimized = false;
    }
    else
    {
        m_maximized = false;
    }
}

bool D3D12Sample::IsDeviceValid() const
{
    return m_device->GetD3D12Device() != nullptr;
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
        float fps = (float)frameCnt;  // fps = frameCnt / 1

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

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_ void D3D12Sample::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 || _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
        {
            m_useWarpDevice = true;
            m_title = m_title + L" (WARP)";
        }
    }
}

// Helper function for resolving the full path of assets.
std::wstring D3D12Sample::GetAssetFullPath(LPCWSTR assetName) const
{
    return m_assetsPath + assetName;
}

// Helper function for setting the window's title text.
void D3D12Sample::SetCustomWindowText(LPCWSTR text) const
{
    std::wstring windowText = m_title + L": " + text;
    SetWindowText(Win32App::GetHwnd(), windowText.c_str());
}

VOID D3D12Sample::CreateCommandObjectsAndInternalFence()
{
    // If we have multiple command queues, we can write a resource only from one queue at the same time.
    // Before it can be accessed by another queue, it must transition to read or common state.
    // In a read state resource can be read from multiple command queues simultaneously, including across processes, based on its read state.
    m_commandQueue = std::make_shared<CommandQueue>(m_device->Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

    m_device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
}

// Present the frame.
VOID D3D12Sample::Present()
{
    m_swapChain->Present();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12Sample::GetCpuSrv(int index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_device->GetHeapStart(), index, m_device->GetDescriptorHandleIncrementSize());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE D3D12Sample::GetGpuSrv(int index) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_device->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), index, m_device->GetDescriptorHandleIncrementSize());
}

CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12Sample::GetDsv(int index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_DSV), index, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12Sample::GetRtv(int index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_device->GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), index, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
}