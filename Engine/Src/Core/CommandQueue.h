#pragma once

#include "Common/DXHelper.h"

/**
 * Wrapper class for a ID3D12CommandQueue.
 */

class CommandQueue
{
public:
    CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
    /*virtual */~CommandQueue(); // gfx, compute, copy

    // Get an available command list from the command queue.
    ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    UINT64 ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> cmdList);

    UINT64 Signal();
    bool IsFenceComplete(UINT64 fenceValue) const;
    void WaitForFenceValue(UINT64 fenceValue);
    void Flush();

    ComPtr<ID3D12CommandQueue> GetCommandQueue() const;

protected:
    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> cmdAllocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        UINT64 fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<ComPtr<ID3D12GraphicsCommandList2>>;

    ComPtr<ID3D12Device2> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    D3D12_COMMAND_LIST_TYPE m_commandListType;
    CommandAllocatorQueue m_commandAllocatorQueue;
    CommandListQueue m_commandListQueue;
};