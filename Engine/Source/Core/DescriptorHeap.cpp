#include "DescriptorHeap.h"
#include "Device.h"

using namespace Scald;
using namespace DirectX;

/* 
 * DescriptorHeapAllocationManager
 */
DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(
    //IMemoryAllocator &Allocator, 
    Device* pDeviceD3D12Impl,
    IDescriptorAllocator *pParentAllocator,
    size_t ThisManagerId,
    const D3D12_DESCRIPTOR_HEAP_DESC &HeapDesc)
    : m_FreeBlockManager(HeapDesc.NumDescriptors)
    , m_HeapDesc(HeapDesc)
    , m_NumDescriptorsInAllocation(HeapDesc.NumDescriptors)
    , m_pDeviceD3D12Impl(pDeviceD3D12Impl)
    , m_pParentAllocator(pParentAllocator)
    , m_ThisManagerId(ThisManagerId)
{
    assert(m_pDeviceD3D12Impl != nullptr && "Invalid device pointer");
    assert(m_pParentAllocator != nullptr && "Invalid parent allocator pointer");
    
    auto pDevice = m_pDeviceD3D12Impl->GetD3D12Device();
    assert(pDevice != nullptr && "Invalid d3d12 device raw pointer");

    m_FirstCPUHandle.ptr = 0u;
    m_FirstGPUHandle.ptr = 0u;
    m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);

    ThrowIfFailed(pDevice->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(m_pd3d12DescriptorHeap.GetAddressOf())));
    m_FirstCPUHandle = m_pd3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    if(m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        m_FirstGPUHandle = m_pd3d12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

// Uses subrange of descriptors in the existing D3D12 descriptor heap
// that starts at offset FirstDescriptor and uses NumDescriptors descriptors
DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(
    Device* pDeviceD3D12Impl,
    IDescriptorAllocator* pParentAllocator,
    size_t ThisManagerId,
    ID3D12DescriptorHeap* pd3d12DescriptorHeap,
    uint32_t FirstDescriptor,
    uint32_t NumDescriptors)
    : m_FreeBlockManager(NumDescriptors)
    , m_pd3d12DescriptorHeap(pd3d12DescriptorHeap)
    , m_NumDescriptorsInAllocation(NumDescriptors)
    , m_pDeviceD3D12Impl(pDeviceD3D12Impl)
    , m_pParentAllocator(pParentAllocator)
    , m_ThisManagerId(ThisManagerId)
{
    assert(m_pDeviceD3D12Impl != nullptr && "Invalid device pointer");
    assert(m_pParentAllocator != nullptr && "Invalid parent allocator pointer");

    m_HeapDesc = m_pd3d12DescriptorHeap->GetDesc();
    m_DescriptorSize = pDeviceD3D12Impl->GetD3D12Device()->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);

    m_FirstCPUHandle = pd3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_FirstCPUHandle.ptr += m_DescriptorSize * FirstDescriptor;

    if (m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        m_FirstGPUHandle = pd3d12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_FirstGPUHandle.ptr += m_DescriptorSize * FirstDescriptor;
    }
}

DescriptorHeapAllocationManager::~DescriptorHeapAllocationManager()
{
    assert((m_FreeBlockManager.GetFreeSize() == m_NumDescriptorsInAllocation, "Not all descriptors were released"));
}

DescriptorHeapAllocation DescriptorHeapAllocationManager::Allocate(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);

    // Use variable-size GPU allocations manager to allocate the requested number of descriptors
    auto DescriptorHandleOffset = m_FreeBlockManager.Allocate(Count);
    if (DescriptorHandleOffset == VariableSizeGPUAllocationsManager::InvalidOffset)
        return DescriptorHeapAllocation();

    // Compute the first CPU and GPU descriptor handles in the allocation by
    // offseting the first CPU and GPU descriptor handle in the range
    auto CPUHandle = m_FirstCPUHandle;
    CPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;
    
    auto GPUHandle = m_FirstGPUHandle;
    if(m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        GPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;

    assert(static_cast<uint16_t>(m_ThisManagerId) < std::numeric_limits<uint16_t>::max() && "ManagerID exceeds 16-bit range");

    return DescriptorHeapAllocation(m_pParentAllocator, m_pd3d12DescriptorHeap.Get(), CPUHandle, GPUHandle, Count, static_cast<uint16_t>(m_ThisManagerId));
}

void DescriptorHeapAllocationManager::Free(DescriptorHeapAllocation&& Allocation)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);

    assert((Allocation.GetAllocationManagerId() == m_ThisManagerId, "Invalid descriptor heap manager Id"));

    auto DescriptorOffset = (Allocation.GetCpuHandle().ptr - m_FirstCPUHandle.ptr) / m_DescriptorSize;

    // Note that the allocation is not released immediately, but added to the release queue in the allocations manager
    m_FreeBlockManager.Free(DescriptorOffset, Allocation.GetNumHandles(), m_pDeviceD3D12Impl->GetCurrentFrame());
    // Clear the allocation
    Allocation = DescriptorHeapAllocation();
}

void DescriptorHeapAllocationManager::ReleaseStaleAllocations(uint64_t NumCompletedFrames)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    m_FreeBlockManager.ReleaseCompletedFrames(NumCompletedFrames);
}

/* 
 * CPUDescriptorHeap
 */

CPUDescriptorHeap::CPUDescriptorHeap(Device* pDeviceD3D12Impl, uint32_t NumDescriptorsInHeap, D3D12_DESCRIPTOR_HEAP_TYPE Type, D3D12_DESCRIPTOR_HEAP_FLAGS Flags)
    : m_pDeviceD3D12Impl(pDeviceD3D12Impl)
    , m_HeapPool()
{
    m_HeapDesc.Type = Type;
    m_HeapDesc.NumDescriptors = NumDescriptorsInHeap;
    m_HeapDesc.Flags = Flags;
    m_HeapDesc.NodeMask = 0u; // multi video adapter stuff

    m_DescriptorSize = m_pDeviceD3D12Impl->GetD3D12Device()->GetDescriptorHandleIncrementSize(Type);

    /*auto descriptorHeapName = L"CPU CBV_SRV_UAV DescriptorHeap";
    if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
        descriptorHeapName = L"CPU SAMPLER DescriptorHeap";
    }
    else if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        descriptorHeapName = L"CPU RTV DescriptorHeap";
    }
    else if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        descriptorHeapName = L"CPU DSV DescriptorHeap";
    }
    SCALD_NAME_D3D12_OBJECT(m_pd3d12DescriptorHeap.Get(), descriptorHeapName);*/
}

CPUDescriptorHeap::~CPUDescriptorHeap()
{
    assert(m_CurrentSize == 0u && "Not all allocations released");
    assert(m_AvailableHeaps.size() == m_HeapPool.size() && "Not all descriptor heap pools are released");

    for (auto HeapPoolIt = m_HeapPool.begin(); HeapPoolIt != m_HeapPool.end(); ++HeapPoolIt)
    {
        assert(HeapPoolIt->GetNumAvailableDescriptors() == m_HeapDesc.NumDescriptors && "Not all descriptors in the descriptor pool are released");
    }
    //LOG_INFO_MESSAGE("Max ", GetD3D12DescriptorHeapTypeLiteralName(m_HeapDesc.Type), " CPU heap size: ", m_MaxHeapSize);
}

DescriptorHeapAllocation CPUDescriptorHeap::Allocate(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    
    DescriptorHeapAllocation Allocation;
    // Go through all descriptor heap managers that have free descriptors
    for (auto AvailableHeapIt = m_AvailableHeaps.begin(); AvailableHeapIt != m_AvailableHeaps.end(); ++AvailableHeapIt)
    {
        // Try to allocate descriptors using the current descriptor heap manager
        Allocation = m_HeapPool[*AvailableHeapIt].Allocate(Count);
        // Remove the manager from the pool if it has no more available descriptors
        if(m_HeapPool[*AvailableHeapIt].GetNumAvailableDescriptors() == 0)
            m_AvailableHeaps.erase(*AvailableHeapIt);

        // Terminate the loop if descriptor was successfully allocated, otherwise
        // go to the next manager
        if(Allocation.GetCpuHandle().ptr != 0u)
            break;
    }

    // If there were no available descriptor heap managers or no manager was able 
    // to suffice the allocation request, create a new manager
    if(Allocation.GetCpuHandle().ptr == 0u)
    {
        // Make sure the heap is large enough to accomodate the requested number of descriptors
        m_HeapDesc.NumDescriptors = std::max(m_HeapDesc.NumDescriptors, static_cast<uint32_t>(Count));
        // Create a new descriptor heap manager. Note that this constructor creates a new D3D12 descriptor
        // heap and references the entire heap. Pool index is used as manager ID
        m_HeapPool.emplace_back(m_pDeviceD3D12Impl, this, m_HeapPool.size(), m_HeapDesc);
        auto NewHeapIt = m_AvailableHeaps.insert(m_HeapPool.size()-1);

        // Use the new manager to allocate descriptor handles
        Allocation = m_HeapPool[*NewHeapIt.first].Allocate(Count);
    }

    m_CurrentSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxHeapSize = std::max(m_MaxHeapSize, m_CurrentSize);

    return Allocation;
}

void CPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    auto ManagerId = Allocation.GetAllocationManagerId();
    m_CurrentSize -= static_cast<uint32_t>(Allocation.GetNumHandles());
    m_HeapPool[ManagerId].Free(std::move(Allocation));
}

void CPUDescriptorHeap::ReleaseStaleAllocations(uint64_t NumCompletedFrames)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    for (size_t HeapManagerInd = 0; HeapManagerInd < m_HeapPool.size(); ++HeapManagerInd)
    {
        m_HeapPool[HeapManagerInd].ReleaseStaleAllocations(NumCompletedFrames);
        // Return the manager to the pool of available managers if it has available descriptors
        if(m_HeapPool[HeapManagerInd].GetNumAvailableDescriptors() > 0)
            m_AvailableHeaps.insert(HeapManagerInd);
    }
}

/* 
 * GPUDescriptorHeap
 */

GPUDescriptorHeap::GPUDescriptorHeap( 
               Device* pDevice, 
               uint32_t NumDescriptorsInHeap, 
               uint32_t NumDynamicDescriptors,
               D3D12_DESCRIPTOR_HEAP_TYPE Type, 
               D3D12_DESCRIPTOR_HEAP_FLAGS Flags)
    : m_HeapDesc
    {
        Type,
        NumDescriptorsInHeap + NumDynamicDescriptors,
        Flags,
        0u // multi video adapter stuff
    }
    , m_pd3d12DescriptorHeap([&]
        {
            ComPtr<ID3D12DescriptorHeap> pHeap;
            ThrowIfFailed(pDevice->GetD3D12Device()->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(pHeap.GetAddressOf())));
            return pHeap;
        }()
    )
    , m_DescriptorSize(pDevice->GetD3D12Device()->GetDescriptorHandleIncrementSize(Type))
    , m_HeapAllocationManager { pDevice, this, 0u, m_pd3d12DescriptorHeap.Get(), 0u, NumDescriptorsInHeap }
    , m_DynamicAllocationsManager { pDevice, this, 0u, m_pd3d12DescriptorHeap.Get(), 0u, NumDynamicDescriptors}
    , m_pDeviceD3D12Impl(pDevice)
{
    auto descriptorHeapName = L"GPU CBV_SRV_UAV DescriptorHeap";
    if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
        descriptorHeapName = L"GPU SAMPLER DescriptorHeap";
    }
    SCALD_NAME_D3D12_OBJECT(m_pd3d12DescriptorHeap.Get(), descriptorHeapName);
}

GPUDescriptorHeap::~GPUDescriptorHeap()
{

}

DescriptorHeapAllocation GPUDescriptorHeap::Allocate(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
    DescriptorHeapAllocation Allocation = m_HeapAllocationManager.Allocate(Count);
    return Allocation;
}

DescriptorHeapAllocation GPUDescriptorHeap::AllocateDynamic(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
    DescriptorHeapAllocation Allocation = m_DynamicAllocationsManager.Allocate(Count);
    return Allocation;
}

void GPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    auto MgrId = Allocation.GetAllocationManagerId();
    // Static allocation
    if (MgrId == 0u)
    {
        std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
        m_HeapAllocationManager.Free(std::move(Allocation));
    }
    // Dynamic allocation
    else
    {
        std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
        m_DynamicAllocationsManager.Free(std::move(Allocation));
    }
}

/*
 * DynamicSuballocationsManager
 */

DynamicSuballocationsManager::DynamicSuballocationsManager(/*IMemoryAllocator& Allocator,*/ GPUDescriptorHeap& ParentGPUHeap, uint32_t DynamicChunkSize)
    : m_ParentGPUHeap(ParentGPUHeap)
    , m_DynamicChunkSize(DynamicChunkSize)
{
}

DynamicSuballocationsManager::~DynamicSuballocationsManager()
{
}

DescriptorHeapAllocation DynamicSuballocationsManager::Allocate(uint32_t Count)
{
    // Check if there are no chunks or the last chunk does not have enough space
    if (m_Suballocations.empty() || m_CurrentSuballocationOffset + Count > m_Suballocations.back().GetNumHandles())
    {
        // Request new chunk from the GPU descriptor heap
        auto SuballocationSize = std::max(m_DynamicChunkSize, Count);
        auto NewDynamicSubAllocation = m_ParentGPUHeap.AllocateDynamic(SuballocationSize);
        m_Suballocations.emplace_back(std::move(NewDynamicSubAllocation));
        m_CurrentSuballocationOffset = 0u;
    }

    // Perform suballocation from the last chunk
    auto& CurrentSuballocation = m_Suballocations.back();

    auto ManagerId = CurrentSuballocation.GetAllocationManagerId();
    DescriptorHeapAllocation Allocation(this, CurrentSuballocation.GetDescriptorHeap(), CurrentSuballocation.GetCpuHandle(m_CurrentSuballocationOffset),
        CurrentSuballocation.GetGpuHandle(m_CurrentSuballocationOffset), Count, static_cast<uint16_t>(ManagerId));
    m_CurrentSuballocationOffset += Count;

    return Allocation;
}

void DynamicSuballocationsManager::Free(DescriptorHeapAllocation&& Allocation)
{
    // Suballocations are not released individually, so this method does nothing.
}
// Instead, all allocations are discarded when command list from this context is recorded and executed by the render device.
void DynamicSuballocationsManager::DiscardAllocations(uint64_t FrameNumber)
{
    // Clear the list and dispose all allocated chunks of GPU descriptor heap.
    m_Suballocations.clear();
}