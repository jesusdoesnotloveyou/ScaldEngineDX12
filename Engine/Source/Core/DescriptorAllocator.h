#include "DXHelper.h"
#include <vector>

namespace Scald
{
    class DescriptorAllocator final
    {
    public:
        DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool bIsShaderVisible = false);
        ~DescriptorAllocator() noexcept;

        D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t* outSlot = nullptr);
        void Free(uint32_t slot);
        ID3D12DescriptorHeap* GetHeap() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetHeapStart() const;

    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
        ID3D12Device* m_device; // Non-owning pointer
        
        std::vector<uint32_t> m_freeSlots;
        D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;

        uint32_t m_numDescriptors = 0u;
        uint32_t m_descriptorSize = 0u;

        D3D12_CPU_DESCRIPTOR_HANDLE m_heapStart;
        D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGpu;
    };
} // namespace Scald