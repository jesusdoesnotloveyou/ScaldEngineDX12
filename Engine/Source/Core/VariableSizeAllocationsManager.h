#include "MemoryAllocator.h"

#include <cstdint>
#include <map>
#include <deque>

namespace Scald
{
    typedef size_t OffsetType;
    struct FreeBlockInfo;
    // Type of the map that keeps memory blocks sorted by their offsets
    using TFreeBlocksByOffsetMap = std::map<OffsetType, FreeBlockInfo>;
    
    // Type of the map that keeps memory blocks sorted by their sizes
    using TFreeBlocksBySizeMap = std::multimap<OffsetType, TFreeBlocksByOffsetMap::iterator>;
    
    struct FreeBlockInfo
    {
        // Block size (no reserved space for the size of allocation)
        OffsetType Size;
    
        // Iterator referencing this block in the multimap sorted by the block size
        TFreeBlocksBySizeMap::iterator OrderBySizeIt;
    
        FreeBlockInfo(OffsetType _Size) : Size(_Size){}
    };
    
    TFreeBlocksByOffsetMap m_FreeBlocksByOffset;
    TFreeBlocksBySizeMap m_FreeBlocksBySize;

    // https://diligentgraphics.com/diligent-engine/architecture/d3d12/variable-size-memory-allocations-manager/
    class VariableSizeAllocationsManager
    {
    public:
        OffsetType Allocate(OffsetType Size);
        void Free(OffsetType Offset, OffsetType Size);

        OffsetType GetFreeSize() const { return m_FreeSize; }

        static const OffsetType InvalidOffset = -1u;

    private:
        void AddNewBlock(OffsetType Offset, OffsetType Size);
    private:
        OffsetType m_FreeSize = 0;
    };

    class VariableSizeGPUAllocationsManager : public VariableSizeAllocationsManager
    {
    private:
        struct FreedAllocationInfo
        {
            OffsetType Offset;
            OffsetType Size;
            uint64_t FrameNumber;
        };

    public:
        VariableSizeGPUAllocationsManager(OffsetType MaxSize, IMemoryAllocator& Allocator);
        ~VariableSizeGPUAllocationsManager();
        VariableSizeGPUAllocationsManager(VariableSizeGPUAllocationsManager&& rhs) noexcept;

        VariableSizeGPUAllocationsManager& operator=(VariableSizeGPUAllocationsManager&& rhs) noexcept = default;
        VariableSizeGPUAllocationsManager(const VariableSizeGPUAllocationsManager&) = delete;
        VariableSizeGPUAllocationsManager& operator=(const VariableSizeGPUAllocationsManager&) = delete;

        void Free(OffsetType Offset, OffsetType Size, uint64_t FrameNumber)
        {
            // Do not release the block immediately, but add
            // it to the queue instead
            m_StaleAllocations.emplace_back(Offset, Size, FrameNumber);
        }

        void ReleaseCompletedFrames(uint64_t NumCompletedFrames)
        {
            // Free all allocations from the beginning of the queue that belong to completed frames
            while(!m_StaleAllocations.empty() && m_StaleAllocations.front().FrameNumber < NumCompletedFrames)
            {
                auto &OldestAllocation = m_StaleAllocations.front();
                VariableSizeAllocationsManager::Free(OldestAllocation.Offset, OldestAllocation.Size);
                m_StaleAllocations.pop_front();
            }
        }

    private:
        std::deque<FreedAllocationInfo> m_StaleAllocations;
    };
}