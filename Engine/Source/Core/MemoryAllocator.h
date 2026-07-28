namespace Scald
{
    class IMemoryAllocator
    {
    public:
        // Allocates block of memory.
        virtual void* Allocate(size_t Size, const char* dbgDescription, const char* dbgFileName, const int dbgLineNumber) = 0;
        // Releases memory.
        virtual void Free(void* Ptr) = 0; 
    };

    class FixedBlockMemoryAllocator final : public IMemoryAllocator
    {
    public:
        FixedBlockMemoryAllocator(size_t BlockSize, size_t BlockCount);
        ~FixedBlockMemoryAllocator();

        virtual void* Allocate(size_t Size, const char* dbgDescription, const char* dbgFileName, const int dbgLineNumber) override;
        virtual void Free(void* Ptr) override;

    private:
        struct Block
        {
            Block* Next;
        };

        Block* FreeList;
        size_t BlockSize;
        size_t BlockCount;
        void* Pool;
    };
}