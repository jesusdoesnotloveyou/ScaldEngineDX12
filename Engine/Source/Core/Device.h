#pragma once

#include "DXHelper.h"
#include "DescriptorHeap.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

struct ID3D12Device2;
struct IDXGIFactory4;
struct IDXGIAdapter1;

namespace Scald
{
    using namespace Microsoft::WRL;

    class SwapChain;
    class CommandQueue;
    class DescriptorAllocator;
    class DynamicUploadHeap;

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
        Device();
    public:
        ~Device() noexcept;
        // NOTE: Enabling the debug layer after device creation will invalidate the active device.
        static void EnableDebugLayer();
        static ComPtr<IDXGIFactory4> CreateFactory();
        // TODO: Must be rewritten to take into account NUMA and UMA GPUs
        static ComPtr<IDXGIAdapter3> CreateAdapter(IDXGIFactory4* factory, bool bUseWarpAdapter = false);
        static ComPtr<ID3D12Device2> CreateDevice(IDXGIAdapter3* adapter);

        static std::unique_ptr<Device> Create();
        std::unique_ptr<SwapChain> CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);
        void CreateCommandObjectsAndInternalFences();

        ID3D12Device2* GetD3D12Device() const { return m_d3d12Device.Get(); }
        IDXGIFactory4* GetDXGIFactory() const { return m_dxgiFactory.Get(); }
        IDXGIAdapter3* GetDXGIAdapter() const { return m_dxgiAdapter.Get(); }
    
        CommandQueue* GetCommandQueue(D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT) const;
        void Flush();
        
        void SafeReleaseD3D12Object(ID3D12Object* pObj);
        uint64_t FinishFrame();
        DynamicUploadHeap* RequestUploadHeap();
        void ReleaseUploadHeap(DynamicUploadHeap* uploadHeap);
        
        uint64_t GetCurrentFrame() const;

    #pragma region DescriptorHeaps
        DescriptorHeapAllocation AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1u);
        DescriptorHeapAllocation AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1u);

        GPUDescriptorHeap& GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type);
        void SetDescriptorsHeaps(ID3D12GraphicsCommandList* pCommandList);

    #pragma endregion DescriptorHeaps

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

        // TODO: move to smth D3D12Factory class
    #pragma region HardwareAndDebugStuff
        void LogAdapters();
        void LogAdapterOutputs(IDXGIAdapter* adapter);
        void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format = DXGI_FORMAT_R10G10B10A2_UNORM);

        static void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);
        void CheckFeatureSupport();
    #pragma endregion HardwareAndDebugStuff

        // TODO: have to call this function at the frame end to release the objects that are no longer used by GPU.
        void ProcessReleasedQueue(bool ForceRelease = false);

    private:
        static inline uint32_t m_dxgiFactoryFlags = 0u;

        // DXGI factory.
        ComPtr<IDXGIFactory4> m_dxgiFactory = nullptr;
        // Adapter info.
        ComPtr<IDXGIAdapter3> m_dxgiAdapter;
        // D3D12 device itself.
        ComPtr<ID3D12Device2> m_d3d12Device = nullptr;
        // ComPtr<ID3D12Device4> m_device4 = nullptr; // For RT stuff
    
        std::unique_ptr<CommandQueue> m_directQueue;
        std::unique_ptr<CommandQueue> m_copyQueue;
        std::unique_ptr<CommandQueue> m_computeQueue;

        // DescriptorHeaps inside. They live on stack.
        CPUDescriptorHeap m_cpuDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
        GPUDescriptorHeap m_gpuDescriptorHeaps[2];  // 0: CBV_SRV_UAV, 1: Sampler

        // Object that must be kept alive
        std::mutex m_releasedObjectsMutex;
        // Release queue
        typedef std::pair<uint64_t, ComPtr<ID3D12Object>> ReleaseQueueElemType;
        std::deque<ReleaseQueueElemType> m_d3d12ObjReleaseQueue;


        std::mutex m_uploadHeapMutex;
        typedef std::unique_ptr<DynamicUploadHeap> UploadHeapPoolElemType;
        std::vector<UploadHeapPoolElemType> m_uploadHeaps;

        // Could be cached.
        uint32_t m_rtvDescriptorSize;
        uint32_t m_dsvDescriptorSize;
        uint32_t m_cbvSrvUavDescriptorSize;

        BOOL UMA = FALSE;
    };
}  // namespace Scald