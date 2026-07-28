#include "DXHelper.h"
#include "VariableSizeAllocationsManager.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <vector>
#include <cassert>

namespace Scald
{      
    class IMemoryAllocator;
    class DescriptorHeapAllocation;
    class DescriptorHeapAllocationManager;
    class Device;
    
    class IDescriptorAllocator
    {
    public:
        // Allocate Count descriptors
        virtual DescriptorHeapAllocation Allocate( uint32_t Count ) = 0;
        virtual void Free(DescriptorHeapAllocation&& Allocation) = 0;
        virtual uint32_t GetDescriptorSize() const = 0;
    };

    class DescriptorHeapAllocation
    {
    public:
        // Creates null allocation
        DescriptorHeapAllocation()
            : m_NumHandles(1u)
            , m_pDescriptorHeap(nullptr)
            , m_DescriptorSize(0u)
        {
            m_FirstCpuHandle.ptr = 0u;
            m_FirstGpuHandle.ptr = 0u; 
        }
        
    
        // Initializes non-null allocation 
        DescriptorHeapAllocation(IDescriptorAllocator* pAllocator,
            ID3D12DescriptorHeap* pHeap,
            D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle,
            uint32_t NumHandles,
            uint16_t AllocationManagerId = static_cast<uint16_t>(-1))
            : m_FirstCpuHandle(CpuHandle)
            , m_FirstGpuHandle(GpuHandle)
            , m_pAllocator(pAllocator)
            , m_pDescriptorHeap(pHeap)
            , m_NumHandles(NumHandles)
            , m_AllocationManagerId(AllocationManagerId)
        {
            assert((m_pAllocator != nullptr && m_pDescriptorHeap != nullptr, "Invalid allocator or descriptor heap"));
            auto DescriptorSize = m_pAllocator->GetDescriptorSize();
            assert((DescriptorSize < std::numeric_limits<uint16_t>::max(), "Descriptor exceeds allowed limit"));
            m_DescriptorSize = static_cast<uint16_t>(DescriptorSize);
        }
        // Move constructor (copy is not allowed)
        DescriptorHeapAllocation(DescriptorHeapAllocation &&Allocation) noexcept
            : m_FirstCpuHandle(Allocation.m_FirstCpuHandle)
            , m_FirstGpuHandle(Allocation.m_FirstGpuHandle)
            , m_pAllocator(std::move(Allocation.m_pAllocator))
            , m_pDescriptorHeap(std::move(Allocation.m_pDescriptorHeap))
            , m_NumHandles(Allocation.m_NumHandles)
            , m_AllocationManagerId(std::move(Allocation.m_AllocationManagerId))
            , m_DescriptorSize(std::move(Allocation.m_DescriptorSize))
        {
            Allocation.m_pAllocator = nullptr;
            Allocation.m_FirstCpuHandle.ptr = 0u;
            Allocation.m_FirstGpuHandle.ptr = 0u;
            Allocation.m_NumHandles = 0u;
            Allocation.m_pDescriptorHeap = nullptr;
            Allocation.m_DescriptorSize = 0u;
            Allocation.m_AllocationManagerId = static_cast<uint16_t>(-1);
        }
    
        // Move assignment (assignment is not allowed)
        DescriptorHeapAllocation& operator=(DescriptorHeapAllocation&& Allocation) noexcept
        {
            m_FirstCpuHandle = Allocation.m_FirstCpuHandle;
            m_FirstGpuHandle = Allocation.m_FirstGpuHandle;
            m_pAllocator = std::move(Allocation.m_pAllocator);
            m_pDescriptorHeap = std::move(Allocation.m_pDescriptorHeap);
            m_NumHandles = Allocation.m_NumHandles;
            m_AllocationManagerId = std::move(Allocation.m_AllocationManagerId);
            m_DescriptorSize = std::move(Allocation.m_DescriptorSize);

            Allocation.m_pAllocator = nullptr;
            Allocation.m_FirstCpuHandle.ptr = 0u;
            Allocation.m_FirstGpuHandle.ptr = 0u;
            Allocation.m_NumHandles = 0u;
            Allocation.m_pDescriptorHeap = nullptr;
            Allocation.m_DescriptorSize = 0u;
            Allocation.m_AllocationManagerId = static_cast<uint16_t>(-1);

            return *this;
        }
    
        // Destructor automatically releases this allocation through the allocator
        ~DescriptorHeapAllocation()
        {
            if(!IsNull() && m_pAllocator)
                m_pAllocator->Free(std::move(*this));

            assert((IsNull(), "Non-null descriptor is being destroyed"));
        }
    
        // Returns CPU descriptor handle at the specified offset
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t Offset = 0u) const 
        { 
            D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = m_FirstCpuHandle; 
            if (Offset != 0)
                CPUHandle.ptr += m_DescriptorSize * Offset;
            return CPUHandle;
        }
    
        // Returns GPU descriptor handle at the specified offset
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t Offset = 0u) const
        { 
            D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = m_FirstGpuHandle;
            if (Offset != 0)
                GPUHandle.ptr += m_DescriptorSize * Offset;
            return GPUHandle;
        }
    
        // Returns pointer to the descriptor heap that contains this allocation
        ID3D12DescriptorHeap* GetDescriptorHeap(){ return m_pDescriptorHeap; }
    
        size_t GetNumHandles() const { return m_NumHandles; }
    
        bool IsNull() const { return m_FirstCpuHandle.ptr == 0; }
        bool IsShaderVisible() const { return m_FirstGpuHandle.ptr != 0; }
        size_t GetAllocationManagerId() { return m_AllocationManagerId; }
        uint32_t GetDescriptorSize()const { return m_DescriptorSize; }
    
    private:
        // No copies, only moves are allowed
        DescriptorHeapAllocation(const DescriptorHeapAllocation&) = delete;
        DescriptorHeapAllocation& operator= (const DescriptorHeapAllocation&) = delete;
    
        // First CPU descriptor handle in this allocation
        D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCpuHandle = {0u};
        
        // First GPU descriptor handle in this allocation
        D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGpuHandle = {0u};
    
        // Pointer to the descriptor heap allocator that created this allocation
        IDescriptorAllocator* m_pAllocator = nullptr;
    
        // Pointer to the D3D12 descriptor heap that contains descriptors in this allocation
        ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
        
        // Number of descriptors in the allocation
        uint32_t m_NumHandles = 0u;
    
        // Allocation manager ID
        uint16_t m_AllocationManagerId = static_cast<uint16_t>(-1);
        
        // Descriptor size 
        uint16_t m_DescriptorSize = 0u;
    };

    class DescriptorHeapAllocationManager
    {
    public:
        // Creates a new D3D12 descriptor heap
        DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                        Device* pDeviceD3D12Impl,
                                        IDescriptorAllocator *pParentAllocator,
                                        size_t ThisManagerId,
                                        const D3D12_DESCRIPTOR_HEAP_DESC &HeapDesc);
    
        // Uses subrange of descriptors in the existing D3D12 descriptor heap
        // that starts at offset FirstDescriptor and uses NumDescriptors descriptors
        DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                        Device *pDeviceD3D12Impl,
                                        IDescriptorAllocator *pParentAllocator,
                                        size_t ThisManagerId,
                                        ID3D12DescriptorHeap *pd3d12DescriptorHeap,
                                        uint32_t FirstDescriptor,
                                        uint32_t NumDescriptors);
    
        // Move constructor
        DescriptorHeapAllocationManager(DescriptorHeapAllocationManager&& rhs) noexcept
            : m_FreeBlockManager(std::move(rhs.m_FreeBlockManager))
            , m_HeapDesc(rhs.m_HeapDesc)
            , m_pd3d12DescriptorHeap(std::move(rhs.m_pd3d12DescriptorHeap))
            , m_FirstCPUHandle(rhs.m_FirstCPUHandle)
            , m_FirstGPUHandle(rhs.m_FirstGPUHandle)
            , m_DescriptorSize(rhs.m_DescriptorSize)
            , m_NumDescriptorsInAllocation(rhs.m_NumDescriptorsInAllocation)
              // Mutex is not movable
              // m_AllocationMutex(std::move(rhs.m_AllocationMutex))
            , m_pDeviceD3D12Impl(rhs.m_pDeviceD3D12Impl)
            , m_pParentAllocator(rhs.m_pParentAllocator)
            , m_ThisManagerId(rhs.m_ThisManagerId)
        {
            rhs.m_FirstCPUHandle.ptr = 0u;
            rhs.m_FirstGPUHandle.ptr = 0u;
            rhs.m_DescriptorSize = 0u;
            rhs.m_NumDescriptorsInAllocation = 0u;
            rhs.m_HeapDesc.NumDescriptors = 0u;
            rhs.m_pDeviceD3D12Impl = nullptr;
            rhs.m_pParentAllocator = nullptr;
            rhs.m_ThisManagerId = static_cast<size_t>(-1);
        }
    
        // No copies or move-assignments
        DescriptorHeapAllocationManager& operator=(DescriptorHeapAllocationManager&& rhs) noexcept = delete;
        DescriptorHeapAllocationManager(const DescriptorHeapAllocationManager&) = delete;
        DescriptorHeapAllocationManager& operator=(const DescriptorHeapAllocationManager&) = delete;
    
        ~DescriptorHeapAllocationManager();
    
        // Allocates Count descriptors
        DescriptorHeapAllocation Allocate(uint32_t Count);
        
        // Releases descriptor heap allocation. Note
        // that the allocation is not released immediately, but
        // added to the release queue in the allocations manager
        void Free(DescriptorHeapAllocation&& Allocation);
        
        // Releases all stale allocation
        void ReleaseStaleAllocations(uint64_t NumCompletedFrames);
    
        size_t GetNumAvailableDescriptors() const { return m_FreeBlockManager.GetFreeSize(); }
    
    private:
        // Allocations manager used to handle descriptor allocations within the heap
        VariableSizeGPUAllocationsManager m_FreeBlockManager;
        
        // Heap description
        D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
    
        // Strong reference to D3D12 descriptor heap object
        ComPtr<ID3D12DescriptorHeap> m_pd3d12DescriptorHeap;
        
        // First CPU descriptor handle in the available descriptor range
        D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCPUHandle = {0u};
        
        // First GPU descriptor handle in the available descriptor range
        D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGPUHandle = {0u};
    
        uint32_t m_DescriptorSize = 0u;
    
        // Number of descriptors in the allocation. 
        // If this manager was initialized as a subrange in the existing heap,
        // this value may be different from m_HeapDesc.NumDescriptors
        uint32_t m_NumDescriptorsInAllocation = 0u;
    
        std::mutex m_AllocationMutex;
        Device *m_pDeviceD3D12Impl = nullptr;
        IDescriptorAllocator *m_pParentAllocator = nullptr;
        
        // External ID assigned to this descriptor allocations manager
        size_t m_ThisManagerId = static_cast<size_t>(-1);
    };

    class CPUDescriptorHeap : public IDescriptorAllocator
    {
    public:
        // Initializes the heap
	    CPUDescriptorHeap(IMemoryAllocator& Allocator, 
                      Device* pDeviceD3D12Impl, 
                      uint32_t NumDescriptorsInHeap, 
                      D3D12_DESCRIPTOR_HEAP_TYPE Type, 
                      D3D12_DESCRIPTOR_HEAP_FLAGS Flags);
        
        CPUDescriptorHeap(const CPUDescriptorHeap&) = delete;
        CPUDescriptorHeap(CPUDescriptorHeap&&) noexcept = delete;
        CPUDescriptorHeap& operator=(const CPUDescriptorHeap&) = delete;
        CPUDescriptorHeap& operator=(CPUDescriptorHeap&&) noexcept = delete;

        ~CPUDescriptorHeap();

        //~ Begin of IDescriptorAllocator interface
        virtual DescriptorHeapAllocation Allocate(uint32_t count) override;
        virtual void Free(DescriptorHeapAllocation&& Allocation) override;
        virtual uint32_t GetDescriptorSize() const override { return m_DescriptorSize; }
        //~ End of IDescriptorAllocator interface

        void ReleaseStaleAllocations(uint64_t NumCompletedFrames);

    protected:
        // Pool of descriptor heap managers
        std::vector<DescriptorHeapAllocationManager> m_HeapPool;
        // Indices of available descriptor heap managers
        std::set<size_t> m_AvailableHeaps;
        IMemoryAllocator& m_MemAllocator;

        std::mutex m_AllocationMutex;

        D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
        uint32_t m_DescriptorSize;
        Device* m_pDeviceD3D12Impl;

        // Maximum heap size during the application lifetime - for statistic purposes
        uint32_t m_CurrentSize = 0u;
        uint32_t m_MaxHeapSize = 0u;  // This size does not count stale allocation
    };

    // https://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-descriptor-heaps/#:~:text=%7D-,GPU%20Descriptor%20Heap,-The%20main%20goal
    class GPUDescriptorHeap : public IDescriptorAllocator
    {
    public:
        GPUDescriptorHeap(IMemoryAllocator &Allocator, 
                      Device* pDevice, 
                      uint32_t NumDescriptorsInHeap, 
                      uint32_t NumDynamicDescriptors,
                      D3D12_DESCRIPTOR_HEAP_TYPE Type, 
                      D3D12_DESCRIPTOR_HEAP_FLAGS Flags);
        
        GPUDescriptorHeap(const GPUDescriptorHeap&) = delete;
        GPUDescriptorHeap(GPUDescriptorHeap&&) noexcept = delete;
        GPUDescriptorHeap& operator=(const GPUDescriptorHeap&) = delete;
        GPUDescriptorHeap& operator=(GPUDescriptorHeap&&) noexcept = delete;
        
        ~GPUDescriptorHeap();

        //~ Begin of IDescriptorAllocator interface
        virtual DescriptorHeapAllocation Allocate(uint32_t count) override;
        virtual void Free(DescriptorHeapAllocation&& Allocation) override;
        virtual uint32_t GetDescriptorSize() const override { return m_DescriptorSize; }
        //~ End of IDescriptorAllocator interface
        
        DescriptorHeapAllocation AllocateDynamic(uint32_t count);

        void ReleaseStaleAllocations(uint64_t NumCompletedFrames);

    protected:
        D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
        ComPtr<ID3D12DescriptorHeap> m_pd3d12DescriptorHeap;

        uint32_t m_DescriptorSize = 0u;

        std::mutex m_AllocMutex, m_DynAllocMutex;

        // Allocation manager for static/mutable part
        DescriptorHeapAllocationManager m_HeapAllocationManager;
        // Allocation manager for dynamic part
        DescriptorHeapAllocationManager m_DynamicAllocationsManager;

        Device* m_pDeviceD3D12Impl;

        uint32_t m_CurrentSize = 0u;
        // Maximum static/mutable part size during the application lifetime - for statistic purposes
        uint32_t m_MaxHeapSize = 0u;

        uint32_t m_CurrentDynamicSize = 0u;
        // Maximum dynamic part size during the application lifetime - for statistic purposes
        uint32_t m_MaxDynamicSize = 0u;
    };


    class DynamicSuballocationsManager : public IDescriptorAllocator
    {
    public:
        DynamicSuballocationsManager(IMemoryAllocator& Allocator, GPUDescriptorHeap& ParentGPUHeap, uint32_t DynamicChunkSize);
        
        DynamicSuballocationsManager(const DynamicSuballocationsManager&) = delete;
        DynamicSuballocationsManager(DynamicSuballocationsManager&&) noexcept = delete;
        DynamicSuballocationsManager& operator=(const DynamicSuballocationsManager&) = delete;
        DynamicSuballocationsManager& operator=(DynamicSuballocationsManager&&) noexcept = delete;

        ~DynamicSuballocationsManager();

        //~ Begin of IDescriptorAllocator interface
        virtual DescriptorHeapAllocation Allocate(uint32_t Count) override;
        virtual void Free(DescriptorHeapAllocation&& Allocation) override;
        virtual uint32_t GetDescriptorSize() const override { return m_ParentGPUHeap.GetDescriptorSize(); }
        //~ End of IDescriptorAllocator interface

        void DiscardAllocations(uint64_t FrameNumber);
        
    private:
        std::vector<DescriptorHeapAllocation> m_Suballocations;
        
        uint32_t m_CurrentSuballocationOffset = 0u;
        uint32_t m_DynamicChunkSize = 0u;

        GPUDescriptorHeap& m_ParentGPUHeap;
    };
}