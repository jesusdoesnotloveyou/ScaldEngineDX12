#pragma once

#include "D3D12Sample.h"
#include "ScaldCoreTypes.h"
#include <vector>

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

using Microsoft::WRL::ComPtr;

class Engine : public D3D12Sample
{
public:
    Engine(UINT width, UINT height, std::wstring name);

    virtual void OnInit() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual void OnDestroy() override;

private:
    static const UINT FrameCount = 2;
    static const UINT TextureWidth = 256u;
    static const UINT TextureHeight = 256u;
    static const UINT TexturePixelSize = 4u; // The number of bytes used to represent a pixel in the texture.

    // Pipeline objects.
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<IDXGIAdapter1> m_hardwareAdapter;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;

    // Synchronization objects.
    ComPtr<ID3D12Fence> m_fence;
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValues[FrameCount];
   
    ComPtr<ID3D12PipelineState> m_pipelineState;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_texture;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGPU();

    std::vector<UINT8> GenerateTextureData();
};