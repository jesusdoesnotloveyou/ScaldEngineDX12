#include "MemoryAllocator.h"

#include <cstdint>
#include <map>
#include <deque>

namespace Scald
{
    // https://diligentgraphics.com/diligent-engine/architecture/d3d12/variable-size-memory-allocations-manager/
    class VariableSizeAllocationsManager
    {
    public:
        typedef size_t OffsetType;
    private:    
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
        
            FreeBlockInfo(OffsetType _Size) : Size(_Size) {}
        };
        
    public:

        struct CreateInfo
        {
            //IMemoryAllocator& Allocator;
            OffsetType MaxSize;
            bool DbgDisableDebugValidation = false;
        };

        explicit VariableSizeAllocationsManager(const CreateInfo& info)
            : m_FreeSize(info.MaxSize)
            , m_MaxSize(info.MaxSize)
            , m_DbgDisableDebugValidation(info.DbgDisableDebugValidation)
        {

        }

        VariableSizeAllocationsManager(OffsetType MaxSize/*, IMemoryAllocator& Allocator*/)
            : VariableSizeAllocationsManager{CreateInfo{/*Allocator,*/ MaxSize}}
        {
        }

        VariableSizeAllocationsManager(VariableSizeAllocationsManager&& rhs) noexcept
            : m_FreeBlocksByOffset(std::move(rhs.m_FreeBlocksByOffset))
            , m_FreeBlocksBySize(std::move(rhs.m_FreeBlocksBySize))
            , m_FreeSize(rhs.m_FreeSize)
            , m_MaxSize(rhs.m_MaxSize)
            , m_DbgDisableDebugValidation(rhs.m_DbgDisableDebugValidation)
        {
            rhs.m_FreeBlocksByOffset.clear();
            rhs.m_FreeBlocksBySize.clear();
            rhs.m_FreeSize = 0u;
            rhs.m_MaxSize = 0u;
        }

        ~VariableSizeAllocationsManager() 
        {

        }

        VariableSizeAllocationsManager& operator=(VariableSizeAllocationsManager&&) = delete;
        VariableSizeAllocationsManager(const VariableSizeAllocationsManager&) = delete;
        VariableSizeAllocationsManager& operator=(const VariableSizeAllocationsManager&) = delete;


        OffsetType Allocate(OffsetType Size);
        void Free(OffsetType Offset, OffsetType Size);

        OffsetType GetFreeSize() const { return m_FreeSize; }

        static const OffsetType InvalidOffset = -1u;

    private:
        void AddNewBlock(OffsetType Offset, OffsetType Size);
    private:
        TFreeBlocksByOffsetMap m_FreeBlocksByOffset;
        TFreeBlocksBySizeMap m_FreeBlocksBySize;

        OffsetType m_FreeSize = 0;
        OffsetType m_MaxSize = 0;

        bool m_DbgDisableDebugValidation = false;
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
        VariableSizeGPUAllocationsManager(OffsetType MaxSize/*, IMemoryAllocator& Allocator*/)
            : VariableSizeAllocationsManager(MaxSize)
        {
        }

        ~VariableSizeGPUAllocationsManager() noexcept
        {
        }

        VariableSizeGPUAllocationsManager(VariableSizeGPUAllocationsManager&& rhs) noexcept
            : VariableSizeAllocationsManager(std::move(rhs))
        {
        }

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