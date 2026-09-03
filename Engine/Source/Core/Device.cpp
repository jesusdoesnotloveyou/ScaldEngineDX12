#include "Device.h"
#include "SwapChain.h"
#include "CommandQueue.h"
#include "DescriptorAllocator.h"
#include "DynamicUploadHeap.h"

#include "GBuffer.h"
#include "SSAO.h"

using namespace Scald;

namespace
{
    const uint32_t kDescriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
        4096u,                                                                                              // CBVSRVUAV
        0u,                                                                                                 // SAMPLER
        RenderCommon::SwapChainFrameCount + GBuffer::EGBufferLayer::MAX - 1u + SSAO::ESSAOTextureType::MAX, // RTV
        3u                                                                                                  // DSV: 1 dsv + 1 csm + 1 gbuffer depth
    };
}   // namespace

Device::Device()
    : m_dxgiFactory(CreateFactory())
    , m_dxgiAdapter(CreateAdapter(m_dxgiFactory.Get()))
    , m_d3d12Device(CreateDevice(m_dxgiAdapter.Get()))
    , m_cpuDescriptorHeaps
        {
            { this, 4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            { this, 2048, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            { this, 8,    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            { this, 3,    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
        }
    , m_gpuDescriptorHeaps
        {
            { this, 256u, 1024u, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },
            { this, 32u,  0u,    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },
        }
{
#if defined(DEBUG) || defined(_DEBUG)
    SCALD_NAME_D3D12_OBJECT(m_d3d12Device, L"Graphics Device");

    CheckFeatureSupport();
    LogAdapters();
#endif
}

Device::~Device() noexcept = default;

// Enable the debug layer (requires the Graphics Tools "optional feature").
// NOTE: Enabling the debug layer after device creation will invalidate the active device.
void Device::EnableDebugLayer()
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

ComPtr<IDXGIFactory4> Device::CreateFactory()
{
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
    return factory;
}

ComPtr<IDXGIAdapter3> Device::CreateAdapter(IDXGIFactory4* factory, bool bUseWarpAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;

    // use UMA video adapter if there is no dedicated
    [[unlikely]]  // C++20
    if (bUseWarpAdapter)
    {   
        // Warp adapter
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
        //ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device)));
    }
    else
    {
        // Hardware adapter
        GetHardwareAdapter(factory, &adapter);
        //ThrowIfFailed(D3D12CreateDevice(m_dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_d3d12Device)));
    }

    ComPtr<IDXGIAdapter3> adapter3;
    ThrowIfFailed(adapter.As(&adapter3));
    return adapter3;
}

ComPtr<ID3D12Device2> Device::CreateDevice(IDXGIAdapter3* adapter)
{
    ComPtr<ID3D12Device2> device;
    
    ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    return device;
}

std::unique_ptr<Device> Device::Create()
{
    return std::unique_ptr<Device>(new Device());
}

std::unique_ptr<SwapChain> Device::CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat)
{
    return std::unique_ptr<SwapChain>(new SwapChain(this, hWnd, width, height, false/*, backBufferFormat*/));
}

void Device::CreateCommandObjectsAndInternalFences()
{
    CreateCommandQueues();
    CreateCommandAllocators();
    CreateCommandLists();
}

void Device::Flush()
{
    m_directQueue->Flush();
    m_copyQueue->Flush();
    m_computeQueue->Flush();
}

uint64_t Device::GetCurrentFrame() const
{
    return m_directQueue->GetFenceValue();
}

DescriptorHeapAllocation Device::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count)
{
    assert(Type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES && "Invalid heap type!");
    return m_cpuDescriptorHeaps[Type].Allocate(Count);
}

DescriptorHeapAllocation Device::AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count)
{
    assert(Type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && "Invalid GPU heap type!");
    return m_gpuDescriptorHeaps[Type].Allocate(Count);
}

GPUDescriptorHeap& Device::GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
    assert(Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && "Invalid GPU descriptor heap type");
    return m_gpuDescriptorHeaps[Type];
}

void Device::SetDescriptorsHeaps(ID3D12GraphicsCommandList* pCommandList)
{
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
        GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap(),
    };
    pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState) 
{
    ThrowIfFailed(m_d3d12Device->CreateGraphicsPipelineState(pDesc, IID_PPV_ARGS(ppPipelineState)));
}

CommandQueue* Device::GetCommandQueue(D3D12_COMMAND_LIST_TYPE commandListType) const
{
    switch (commandListType)
    {
        case D3D12_COMMAND_LIST_TYPE_DIRECT: return m_directQueue.get(); break;
        case D3D12_COMMAND_LIST_TYPE_COPY: return m_copyQueue.get(); break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_computeQueue.get(); break;
        default: assert(false && "Invalid command queue type");
    }
    return nullptr;
}

void Device::CreateCommandQueues()
{
    // If we have multiple command queues, we can write a resource only from one queue at the same time.
    // Before it can be accessed by another queue, it must transition a resource to read or common state.
    // In a read state resource can be read from multiple command queues simultaneously, including across processes, based on its read state.
    m_directQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this->GetD3D12Device()));
    m_copyQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this->GetD3D12Device(), D3D12_COMMAND_LIST_TYPE_COPY));
    m_computeQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this->GetD3D12Device(), D3D12_COMMAND_LIST_TYPE_COMPUTE));
}

void Device::CreateCommandAllocators() {}

void Device::CreateCommandLists() {}

void Device::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while (m_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
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

void Device::LogAdapterOutputs(IDXGIAdapter* adapter)
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

        // TODO : remove hardcode
        LogOutputDisplayModes(output /*, BackBufferFormat*/);

        SAFE_RELEASE(output);
        ++i;
    }
}

void Device::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
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
            L"Width = " + std::to_wstring(x.Width) + L" " + L"Height = " + std::to_wstring(x.Height) + L" " + L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) + L"\n";

        ::OutputDebugString(text.c_str());
    }
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_ void Device::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT adapterIndex = 0u; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                 adapterIndex, requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter)));
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

void Device::CheckFeatureSupport()
{
    D3D12_FEATURE_DATA_ARCHITECTURE architectureInfo = {};
    if (SUCCEEDED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &architectureInfo, sizeof(architectureInfo))))
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

void Device::ProcessReleasedQueue(bool ForceRelease)
{
    //std::lock_guard<std::mutex> LockGuard(m_releasedObjectsMutex);

    //// Release all objects whose frame number value < number of completed frames
    //while (!m_d3d12ObjReleaseQueue.empty())
    //{
    //    auto& FirstObj = m_d3d12ObjReleaseQueue.front();
    //    // GPU must have been idled when ForceRelease == true
    //    if (FirstObj.first < m_NumCompletedFrames || ForceRelease)
    //        m_d3d12ObjReleaseQueue.pop_front();
    //    else
    //        break;
    //}
}