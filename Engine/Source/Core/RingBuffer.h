#pragma once

#include "DXHelper.h"
#include <queue>
#include <cstdint>

namespace Scald
{
    class RingBuffer
    {
    public:
        typedef size_t OffsetType;
        struct FrameTailAttribs
        {
            FrameTailAttribs(uint64_t fv, OffsetType off, OffsetType sz)
            : FenceValue(fv)
            , Offset(off)
            , Size(sz)
            {}
            // Fence value associated with the command list in which
            // the allocation could have been referenced last time
            uint64_t FenceValue;
            OffsetType Offset;
            OffsetType Size;
        };
        static const OffsetType InvalidOffset = static_cast<OffsetType>(-1);
        
        RingBuffer(OffsetType MaxSize /*,IMemoryAllocator& Allocator*/);
        RingBuffer(RingBuffer&& rhs) noexcept = default;
        RingBuffer& operator=(RingBuffer&& rhs) noexcept = default;
        
        virtual ~RingBuffer() noexcept;

        RingBuffer(const RingBuffer&) = delete;
        RingBuffer& operator=(const RingBuffer&) = delete;

        OffsetType Allocate(OffsetType Size);
        
        void FinishCurrentFrame(uint64_t FenceValue);
        void ReleaseCompletedFrames(uint64_t CompletedFenceValue);
        
        OffsetType GetMaxSize() const { return m_MaxSize; }
        bool IsFull() const { return m_UsedSize == m_MaxSize; }
        bool IsEmpty() const { return m_UsedSize == 0; }
        OffsetType GetUsedSize() const { return m_UsedSize; }
    
    private:
        std::deque<FrameTailAttribs> m_CompletedFrameTails;
        OffsetType m_Head = 0;
        OffsetType m_Tail = 0;
        OffsetType m_MaxSize = 0;
        OffsetType m_UsedSize = 0;
        OffsetType m_CurrFrameSize = 0;
    };
} // namespace Scald