#include "DescriptorAllocator.h"

#include <numeric>

using namespace Scald;
using namespace DirectX;

DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool bIsShaderVisible)
    : m_device(device)
    , m_heapType(heapType)
    , m_numDescriptors(numDescriptors)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = heapType;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = bIsShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0u; // multi adapter stuff

    m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
    m_heapStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(heapType);
    m_heapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_freeSlots.resize(numDescriptors);
    std::iota(m_freeSlots.begin(), m_freeSlots.end(), 0u);
}

DescriptorAllocator::~DescriptorAllocator() noexcept = default;

ID3D12DescriptorHeap* DescriptorAllocator::GetHeap() const
{
    return m_heap.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetHeapStart() const
{
    return m_heapStart;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t* outSlot)
{
    assert(!m_freeSlots.empty() && "No free handles available");

    uint32_t slot = m_freeSlots.back();
    m_freeSlots.pop_back();
    if (outSlot)
        *outSlot = slot;

    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heapStart, slot, m_descriptorSize);
}

void DescriptorAllocator::Free(uint32_t slot)
{
    m_freeSlots.push_back(slot);
    // if (handle.ptr < m_heapStart.ptr || handle.ptr >= m_heapStart.ptr + m_numDescriptors * m_device->GetDescriptorHandleIncrementSize(m_heapType))
    // {
    //     return;
    // }

    // m_freeHandles.push_back((handle.ptr - m_heapStart.ptr) / m_device->GetDescriptorHandleIncrementSize(m_heapType));
}
