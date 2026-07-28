#pragma once

#include "DXHelper.h"

#include <cstdint>
#include <memory>

struct ID3D12Device2;
struct IDXGIFactory4;
struct IDXGIAdapter1;

namespace Scald
{
using namespace Microsoft::WRL;

class SwapChain;
class CommandQueue;
class DescriptorAllocator;

enum class QueueType : uint8_t
{
    Direct,
    Copy,
    Compute,
    QueueTypes = 3
};

class Device final /*: std::enable_shared_from_this<Device>*/
{
private:
    Device(bool bUseWarpAdapter);
public:
    ~Device() noexcept;
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    static void EnableDebugLayer();
    
    static std::unique_ptr<Device> Create(bool bUseWarpAdapter = false);
    std::unique_ptr<SwapChain> CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);
    void CreateCommandObjectsAndInternalFences();

    ID3D12Device2* Get() const
    {
        return m_d3d12Device.Get();
    }

    ComPtr<ID3D12Device2> GetD3D12Device() const
    {
        return m_d3d12Device;
    }

    ComPtr<IDXGIFactory4> GetDXGIFactory() const
    {
        return m_dxgiFactory;
    }

    ComPtr<IDXGIAdapter3> GetDXGIAdapter() const
    {
        return m_dxgiAdapter;
    }
    
    CommandQueue* GetCommandQueue(D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    void Flush();

    uint64_t GetCurrentFrame() const;

#pragma endregion DescriptorHeaps
    // Create RTV, DSV and SRV descriptor heaps.
    // Descriptor has to be created for every GPU resource.
    void CreateDescriptorHeaps();

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTV(uint32_t* slot = nullptr);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSV(uint32_t* slot = nullptr);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRV(uint32_t* slot = nullptr);

    void FreeRTV(uint32_t slot);
    void FreeDSV(uint32_t slot);
    void FreeSRV(uint32_t slot);

    ID3D12DescriptorHeap* GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetHeapStart(D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) const;
    
    uint32_t GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) const;

#pragma region DescriptorHandles
    void CreateShaderResourceView(ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE destDescriptor);
#pragma endregion DescriptorHandles

#pragma region PSO
    void CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, ID3D12PipelineState** ppPipelineState);
#pragma endregion PSO

private:
#pragma region CommandObjects
    void CreateCommandQueues();
    void CreateCommandAllocators();
    void CreateCommandLists();

    // TODO: have to call GPUDescriptorHeaps::ReleaseStaleAllocations()
    void CloseAndExecuteCommandContext();
#pragma endregion CommandObjects

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format = DXGI_FORMAT_R10G10B10A2_UNORM);

    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);
    void CheckFeatureSupport();

private:
    static inline uint32_t m_dxgiFactoryFlags = 0u;
    
    std::unique_ptr<CommandQueue> m_directQueue;
    std::unique_ptr<CommandQueue> m_copyQueue;
    std::unique_ptr<CommandQueue> m_computeQueue;

    // Adapter info.
    ComPtr<IDXGIAdapter3> m_dxgiAdapter;
    // DXGI factory.
    ComPtr<IDXGIFactory4> m_dxgiFactory = nullptr;
    // D3D12 device itself.
    ComPtr<ID3D12Device2> m_d3d12Device = nullptr;
    // ComPtr<ID3D12Device4> m_device4 = nullptr; // For RT stuff

    // DescriptorHeaps inside.
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    // Could be cached.
    uint32_t m_rtvDescriptorSize;
    uint32_t m_dsvDescriptorSize;
    uint32_t m_cbvSrvUavDescriptorSize;

    BOOL UMA = FALSE;
};
}  // namespace Scald