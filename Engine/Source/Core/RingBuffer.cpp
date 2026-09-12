#include "RingBuffer.h"

using namespace Scald;

RingBuffer::RingBuffer(OffsetType MaxSize /*,IMemoryAllocator& Allocator*/)
    : m_MaxSize(MaxSize)
{

}

RingBuffer::~RingBuffer()
{
}

RingBuffer::OffsetType RingBuffer::Allocate(OffsetType Size)
{
	if (IsFull()) return InvalidOffset;

	if (m_Tail >= m_Head)
	{
        //               Head             Tail     MaxSize
        //               |                |        |
        //  [            xxxxxxxxxxxxxxxxx         ]
        //
        //
        if (m_Tail + Size <= m_MaxSize)
        {
            auto Offset = m_Tail;
            m_Tail += Size;
            m_UsedSize += Size;
            m_CurrFrameSize += Size;
            return Offset;
        }
        else if (Size <= m_Head)
        {
            // Allocate from the beginning of the buffer
            OffsetType AddSize = (m_MaxSize - m_Tail) + Size;
            m_UsedSize += AddSize;
            m_CurrFrameSize += AddSize;
            m_Tail = Size;
            return 0;
        }
	}
    else if (m_Tail + Size <= m_Head)
    {
        //
        //       Tail          Head
        //       |             |
        //  [xxxx              xxxxxxxxxxxxxxxxxxxxxxxxxx]
        //
        auto Offset = m_Tail;
        m_Tail += Size;
        m_UsedSize += Size;
        m_CurrFrameSize += Size;
        return Offset;
    }
    
    return InvalidOffset;
}

void RingBuffer::FinishCurrentFrame(uint64_t FenceValue)
{
    m_CompletedFrameTails.emplace_back(FenceValue, m_Tail, m_CurrFrameSize);
    m_CurrFrameSize = 0u;
}

void RingBuffer::ReleaseCompletedFrames(uint64_t CompletedFenceValue)
{
    // We can release all tails whose associated fence value is less than or equal to CompletedFenceValue
    while (!m_CompletedFrameTails.empty() && m_CompletedFrameTails.front().FenceValue <= CompletedFenceValue)
    {
        const auto& OldestFrameTail = m_CompletedFrameTails.front();
        assert(OldestFrameTail.Size <= m_UsedSize && "OldestFrameTail must be inside m_UsedSize");
        m_UsedSize -= OldestFrameTail.Size;
        m_Head = OldestFrameTail.Offset;
        m_CompletedFrameTails.pop_front();
    }
}