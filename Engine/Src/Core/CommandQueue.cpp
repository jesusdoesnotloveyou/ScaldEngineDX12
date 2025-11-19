#pragma once

#include "stdafx.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
    : m_fenceValue((UINT64)0u)
    , m_commandListType(type)
    , m_device(device)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0u;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(m_device->CreateFence(m_fenceValue, 
        /*D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,*/
        D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    //assert(m_fenceEvent && "Failed to create fence event handle.");
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

CommandQueue::~CommandQueue()
{
    CloseHandle(m_fenceEvent);
}

UINT64 CommandQueue::Signal()
{
    ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    return m_fenceValue;
}

bool CommandQueue::IsFenceComplete(UINT64 fenceValue) const
{
    /*
     * GetCompletedValue()
     * Returns the current value of the fence. If the device has been removed, the return value will be UINT64_MAX.
    */
    return fenceValue <= m_fence->GetCompletedValue();
}

void CommandQueue::WaitForFenceValue(UINT64 fenceValue)
{
    if (!IsFenceComplete(fenceValue))
    {
        /*
         * SetEventOnCompletion(fenceValue, fenceEvent)
         * Specifies an event that's raised when the fence reaches a certain value.
        */
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE); // WaitForSingleObjecEx(m_fenceEvent, INFINITE, FALSE)
    }
}

void CommandQueue::Flush()
{
    WaitForFenceValue(Signal());
}

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(ID3D12CommandAllocator* pCommandAllocator)
{
    ComPtr<ID3D12GraphicsCommandList2> commandList;
    ThrowIfFailed(m_device->CreateCommandList(0u, m_commandListType, pCommandAllocator, nullptr, IID_PPV_ARGS(&commandList)));

    return commandList;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList(ID3D12CommandAllocator* pCommandAllocator)
{
    ComPtr<ID3D12GraphicsCommandList2> commandList;

    if (!m_commandListQueue.empty())
    {
        commandList = m_commandListQueue.front();
        m_commandListQueue.pop();

        ThrowIfFailed(commandList->Reset(pCommandAllocator, nullptr));
    }
    else
    {
        commandList = CreateCommandList(pCommandAllocator);
    }

    return commandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
UINT64 CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    commandList->Close();

    ID3D12CommandList* const ppCommandLists[] = { commandList.Get() };

    m_commandQueue->ExecuteCommandLists(1u, ppCommandLists);
    UINT64 fenceValue = Signal();

    m_commandListQueue.push(commandList);

    return fenceValue;
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const
{
    return m_commandQueue;
}