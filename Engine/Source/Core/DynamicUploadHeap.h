#pragma once

#include "DXHelper.h"
#include "RingBuffer.h"

namespace Scald
{
    // Constant blocks must be multiples of 16 constants @ 16 bytes each
    #define DEFAULT_ALIGN 256

    class Device;
    
    struct DynamicAllocation
    {
        DynamicAllocation(ID3D12Resource* pBuff = nullptr, size_t ThisOffset = 0u, size_t ThisSize = 0u)
            : pBuffer(pBuff)
            , Offset(ThisOffset)
            , Size(ThisSize)
        {}

        ID3D12Resource* pBuffer = nullptr;
        size_t Offset = 0u;
        size_t Size = 0u;
        void* CPUAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0u;

    #if defined(DEBUG) || defined(_DEBUG)
        uint64_t FrameNum = static_cast<uint64_t>(-1);
    #endif
    };

    class GPURingBuffer final : public RingBuffer
    {
    public:
        GPURingBuffer(size_t MaxSize, Device* pd3d12Device, bool bAllowCpuAccess);

        GPURingBuffer(GPURingBuffer&& rhs) noexcept
            : RingBuffer(std::move(rhs))
            , m_CpuVirtualAddress(rhs.m_CpuVirtualAddress)
            , m_GpuVirtualAddress(rhs.m_GpuVirtualAddress)
            , m_pBuffer(std::move(rhs.m_pBuffer))
        {
            rhs.m_CpuVirtualAddress = nullptr; 
            rhs.m_GpuVirtualAddress = 0u; 
            // Probably redundant because of the ComPtr move ctor
            rhs.m_pBuffer.Reset();
        }

        GPURingBuffer& operator=(GPURingBuffer&& rhs) noexcept
        { 
            Destroy();

            static_cast<RingBuffer&>(*this) = std::move(rhs);
            m_CpuVirtualAddress = rhs.m_CpuVirtualAddress;
            m_GpuVirtualAddress = rhs.m_GpuVirtualAddress;
            m_pBuffer = std::move(rhs.m_pBuffer);
            rhs.m_CpuVirtualAddress = nullptr;
            rhs.m_GpuVirtualAddress = 0u;

            return *this;
        }

        ~GPURingBuffer() noexcept override;

        DynamicAllocation Allocate(size_t SizeInBytes)
        {
            auto Offset = RingBuffer::Allocate(SizeInBytes);
            if (Offset == RingBuffer::InvalidOffset) return DynamicAllocation(nullptr, 0u, 0u);
             
            DynamicAllocation DynAlloc(m_pBuffer.Get(), Offset, SizeInBytes);
            DynAlloc.GPUAddress = m_GpuVirtualAddress + Offset;
            DynAlloc.CPUAddress = m_CpuVirtualAddress;
            if (DynAlloc.CPUAddress)
            {
                DynAlloc.CPUAddress = reinterpret_cast<char*>(DynAlloc.CPUAddress) + Offset;
            }            
            return DynAlloc;
        }

        GPURingBuffer(const GPURingBuffer&) = delete;
        GPURingBuffer& operator=(GPURingBuffer&) = delete;
    private:
        void Destroy();

    private:
        void* m_CpuVirtualAddress;
        D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
        ComPtr<ID3D12Resource> m_pBuffer;
    };

    class DynamicUploadHeap // final : public NonCopyable
    {
    public:
        DynamicUploadHeap(bool bIsCpuAccessible, Device* device, size_t initialSize);
        ~DynamicUploadHeap() noexcept;

        DynamicAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

        void FinishFrame(uint64_t FrameNum, uint64_t NumCompletedFrames);

        DynamicUploadHeap(const DynamicUploadHeap& lhs) = delete;
        DynamicUploadHeap& operator=(const DynamicUploadHeap& lhs) = delete;
        DynamicUploadHeap(DynamicUploadHeap&& rhs) noexcept = delete;
        DynamicUploadHeap& operator=(DynamicUploadHeap&& rhs) noexcept = delete;
    private:
        const bool m_bIsCpuAccessible;
        // When a chunk of dynamic memory is requested, the heap first tries to allocate the memory in the largest GPU buffer.
        // If allocation fails, it a new ring buffer is created that provides enough space and requests memory from that buffer.
        // Only the largest buffer is used for allocation and all other buffers are released when GPU is done with corresponding frames
        std::vector<GPURingBuffer> m_ringBuffers;

        Device* m_device;
    };
}