#include "VariableSizeAllocationsManager.h"

using namespace Scald;

OffsetType VariableSizeAllocationsManager::Allocate(OffsetType Size)
{
    if(m_FreeSize < Size)
        return InvalidOffset;

    // Get the first block that is large enough to encompass Size bytes
    auto SmallestBlockItIt = m_FreeBlocksBySize.lower_bound(Size);
    if(SmallestBlockItIt == m_FreeBlocksBySize.end())
        return InvalidOffset;

    auto SmallestBlockIt = SmallestBlockItIt->second;
    auto Offset = SmallestBlockIt->first;
    auto NewOffset = Offset + Size;
    auto NewSize = SmallestBlockIt->second.Size - Size;
    m_FreeBlocksBySize.erase(SmallestBlockItIt);
    m_FreeBlocksByOffset.erase(SmallestBlockIt);
    if (NewSize > 0)
        AddNewBlock(NewOffset, NewSize);

    m_FreeSize -= Size;
    return Offset;
}

void VariableSizeAllocationsManager::Free(OffsetType Offset, OffsetType Size)
{
    // Find the first element whose offset is greater than the specified offset
    auto NextBlockIt = m_FreeBlocksByOffset.upper_bound(Offset);
    auto PrevBlockIt = NextBlockIt;
    if(PrevBlockIt != m_FreeBlocksByOffset.begin())
        --PrevBlockIt;
    else
        PrevBlockIt = m_FreeBlocksByOffset.end();
 
    OffsetType NewSize, NewOffset;
    if(PrevBlockIt != m_FreeBlocksByOffset.end() && Offset == PrevBlockIt->first + PrevBlockIt->second.Size)
    {
        // PrevBlock.Offset           Offset
        // |                          |
        // |<-----PrevBlock.Size----->|<------Size-------->|
        //
        NewSize = PrevBlockIt->second.Size + Size;
        NewOffset = PrevBlockIt->first;
 
        if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
        {
            // PrevBlock.Offset           Offset               NextBlock.Offset 
            // |                          |                    |
            // |<-----PrevBlock.Size----->|<------Size-------->|<-----NextBlock.Size----->|
            //
            NewSize += NextBlockIt->second.Size;
            m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
            m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
            // Delete the range of two blocks
            ++NextBlockIt;
            m_FreeBlocksByOffset.erase(PrevBlockIt, NextBlockIt);
        }
        else
        {
            // PrevBlock.Offset           Offset                       NextBlock.Offset 
            // |                          |                            |
            // |<-----PrevBlock.Size----->|<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
            //
            m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
            m_FreeBlocksByOffset.erase(PrevBlockIt);
        }
    }
    else if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
    {
        // PrevBlock.Offset                   Offset               NextBlock.Offset 
        // |                                  |                    |
        // |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->|<-----NextBlock.Size----->|
        //
        NewSize = Size + NextBlockIt->second.Size;
        NewOffset = Offset;
        m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
        m_FreeBlocksByOffset.erase(NextBlockIt);
    }
    else
    {
        // PrevBlock.Offset                   Offset                       NextBlock.Offset 
        // |                                  |                            |
        // |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
        //
        NewSize = Size;
        NewOffset = Offset;
    }
 
    AddNewBlock(NewOffset, NewSize);
 
    m_FreeSize += Size;
}

void VariableSizeAllocationsManager::AddNewBlock(OffsetType Offset, OffsetType Size)
{
    auto NewBlockIt = m_FreeBlocksByOffset.emplace(Offset, Size);
    auto OrderIt = m_FreeBlocksBySize.emplace(Size, NewBlockIt.first);
    NewBlockIt.first->second.OrderBySizeIt = OrderIt;
}
