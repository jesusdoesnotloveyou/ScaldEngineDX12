#include "Device.h"
#include "SwapChain.h"
#include "CommandQueue.h"

using namespace Scald;

Device::Device(bool bUseWarpAdapter)
{
#if defined(DEBUG) || defined(_DEBUG)
    CreateDebugLayer();
#endif

    m_useWarpDevice = bUseWarpAdapter;

    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // use UMA video adapter if there is no dedicated
    [[unlikely]]  // C++20
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
    LogAdapters();
#endif
}

// Enable the debug layer (requires the Graphics Tools "optional feature").
// NOTE: Enabling the debug layer after device creation will invalidate the active device.
void Device::CreateDebugLayer()
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

std::unique_ptr<Device> Device::Create(bool bUseWarpAdapter)
{
    return std::unique_ptr<Device>(new Device(bUseWarpAdapter));
}

std::unique_ptr<SwapChain> Device::CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat)
{
    return std::unique_ptr<SwapChain>(new SwapChain(this, hWnd, width, height, backBufferFormat));
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

uint32_t Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const
{
    return m_device->GetDescriptorHandleIncrementSize(heapType);
}

std::shared_ptr<CommandQueue> Device::GetCommandQueue(D3D12_COMMAND_LIST_TYPE commandListType) const
{
    return std::shared_ptr<CommandQueue>();
}

void Device::CreateCommandQueues()
{
    m_directQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this));
    m_copyQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this, D3D12_COMMAND_LIST_TYPE_COPY));
    m_computeQueue = std::unique_ptr<CommandQueue>(new CommandQueue(this, D3D12_COMMAND_LIST_TYPE_COMPUTE));
}

void Device::CreateCommandAllocators() {}

void Device::CreateCommandLists() {}

void Device::LogAdapters()
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
        LogOutputDisplayModes(output/*, BackBufferFormat*/);

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
_Use_decl_annotations_
void Device::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
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