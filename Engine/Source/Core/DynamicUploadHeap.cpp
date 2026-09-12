#include "DynamicUploadHeap.h"
#include "Device.h"

using namespace Scald;

GPURingBuffer::GPURingBuffer(size_t MaxSize, /*IMemoryAllocator& Allocator,*/ Device* pd3d12Device, bool bAllowCpuAccess)
    : RingBuffer(MaxSize /*, Allocator*/)
    , m_CpuVirtualAddress(nullptr)
    , m_GpuVirtualAddress(0u)
    , m_pBuffer(nullptr)
{
    // Almost the same as in UploadBuffer constructor, but I used d3x12.h wrappers there
    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1u;
    heapProps.VisibleNodeMask = 1u;

    D3D12_RESOURCE_DESC resourceDesc;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = static_cast<UINT64>(0u);
    resourceDesc.Height = 1u;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(1u);
    resourceDesc.MipLevels = static_cast<UINT16>(1u);
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.SampleDesc.Quality = 0u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_RESOURCE_STATES defaultUsage;
    if (bAllowCpuAccess)  // CPU accessible memory, GPU can read from it, but cannot write to it
    {
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
        
    else
    {
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        defaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    resourceDesc.Width = MaxSize;

    HRESULT hr = pd3d12Device->GetD3D12Device()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, defaultUsage, nullptr, IID_PPV_ARGS(m_pBuffer.GetAddressOf()));

    // TODO: handle the error properly, maybe throw an exception or return an error code
    assert(SUCCEEDED(hr) && "Failed to create new upload ring buffer");

    m_pBuffer->SetName(L"Upload Ring Buffer");

    m_GpuVirtualAddress = m_pBuffer->GetGPUVirtualAddress();
    if (bAllowCpuAccess)
    {
        m_pBuffer->Map(0, nullptr, &m_CpuVirtualAddress);
    }
}

void GPURingBuffer::Destroy()
{
    if (m_CpuVirtualAddress)
    {
        m_pBuffer->Unmap(0u, nullptr);
    }
    m_CpuVirtualAddress = nullptr;
    m_GpuVirtualAddress = 0u;
    m_pBuffer.Reset();
}

GPURingBuffer::~GPURingBuffer() noexcept
{
    Destroy();
}

DynamicUploadHeap::DynamicUploadHeap(/*IMemoryAllocator& Allocator,*/ bool bIsCpuAccessible, Device* device, size_t initialSize)
    : m_bIsCpuAccessible(bIsCpuAccessible)
    , m_device(device)
{
    m_ringBuffers.emplace_back(initialSize, /*Allocator,*/ device, bIsCpuAccessible);
}

DynamicUploadHeap::~DynamicUploadHeap() noexcept {}

DynamicAllocation DynamicUploadHeap::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
{
    // This is very expensive! Currently other threads cannot allocate dynamic
    // data while frame is being finished.
    // std::lock_guard<std::mutex> Lock(m_Mutex);

    const size_t AlignmentMask = Alignment - 1u;
    // Assert that it's a power of two.
    assert((AlignmentMask & Alignment) == 0);
    // Align the allocation
    const size_t AlignedSize = (SizeInBytes + AlignmentMask) & ~AlignmentMask;
    auto DynAlloc = m_ringBuffers.back().Allocate(AlignedSize);
    if (!DynAlloc.pBuffer)
    {
        auto NewMaxSize = m_ringBuffers.back().GetMaxSize() * 2;
        while (NewMaxSize < SizeInBytes)
            NewMaxSize *= 2;
        m_ringBuffers.emplace_back(NewMaxSize, /*m_Allocator,*/ m_device, m_bIsCpuAccessible);
        DynAlloc = m_ringBuffers.back().Allocate(AlignedSize);
    }
#ifdef _DEBUG
    DynAlloc.FrameNum = m_device->GetCurrentFrame();
#endif
    return DynAlloc;
}

void DynamicUploadHeap::FinishFrame(uint64_t FrameNum, uint64_t NumCompletedFrames)
{
    // std::lock_guard<std::mutex> Lock(m_Mutex);

    size_t NumBuffsToDelete = 0;

    for (size_t Ind = 0; Ind < m_ringBuffers.size(); ++Ind)
    {
        auto& ringBuff = m_ringBuffers[Ind];
        ringBuff.FinishCurrentFrame(FrameNum);
        ringBuff.ReleaseCompletedFrames(NumCompletedFrames);
        if (NumBuffsToDelete == Ind && Ind < m_ringBuffers.size() - 1 && ringBuff.IsEmpty())
        {
            ++NumBuffsToDelete;
        }
    }

    if (NumBuffsToDelete) 
        m_ringBuffers.erase(m_ringBuffers.begin(), m_ringBuffers.begin() + NumBuffsToDelete);
}
