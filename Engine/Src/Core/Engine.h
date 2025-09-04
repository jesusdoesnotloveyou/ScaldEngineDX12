#pragma once

#include "D3D12Sample.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "Camera.h"
#include <vector>

const int gNumFrameResources = 3;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

using Microsoft::WRL::ComPtr;

// F. Luna stuff: lightweight structure that stores parameters to draw a shape.
struct RenderItem
{
    RenderItem() = default;
    XMMATRIX World = XMMatrixIdentity();

    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class Engine : public D3D12Sample
{
public:
    Engine(UINT width, UINT height, std::wstring name, std::wstring className);
    virtual ~Engine() override;

    virtual void OnInit() override;
    virtual void OnUpdate(const ScaldTimer& st) override;
    virtual void OnRender(const ScaldTimer& st) override;
    virtual void OnDestroy() override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    void OnKeyboardInput(const ScaldTimer& st);
    void UpdateObjectsCB(const ScaldTimer& st);
    void UpdatePassCB(const ScaldTimer& st);
    
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems);

private:
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;
    FrameResource* m_currFrameResource = nullptr;
    int m_currFrameResourceIndex = 0;

    UINT m_passCbvOffset = 0;

    static const UINT TextureWidth = 256u;
    static const UINT TextureHeight = 256u;
    static const UINT TexturePixelSize = 4u; // The number of bytes used to represent a pixel in the texture.

    static const UINT FrameCount = 2;
    static const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // Pipeline objects.
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;
    //ComPtr<ID3D12DescriptorHeap> m_srvHeap; // Texture
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap; // heap for constant buffer views
    UINT m_cbvSrvUavDescriptorSize = 0u;

    // Synchronization objects.
    UINT m_frameIndex = 0u; // keep track of front and back buffers (see FrameCount)
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValues[FrameCount];
   
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_pipelineStates;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    ObjectConstants m_objectConstantBufferData;
    PassConstants m_passConstantBufferData;

    //ComPtr<ID3D12Resource> m_texture;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
    std::vector<std::unique_ptr<RenderItem>> m_renderItems;
    std::vector<RenderItem*> m_opaqueItems;

    std::unique_ptr<Camera> m_camera;

    //XMMATRIX mWorld = XMMatrixIdentity();
    XMMATRIX mView = XMMatrixIdentity();
    XMMATRIX mProj = XMMatrixIdentity();

    VOID LoadPipeline();
    VOID CreateDebugLayer();
    VOID CreateDevice();
    VOID CreateCommandObjects();
    VOID CreateFence();
    VOID CreateRtvAndDsvDescriptorHeaps();
    VOID CreateSwapChain();
    
    VOID Reset() override;
    VOID FlushCommandQueue();
    
    VOID LoadAssets();
    VOID CreateRootSignature();
    VOID CreateShaders();
    VOID CreatePSO();
    // Shapes
    VOID CreateGeometry();
    // Shapes could constist of some items to render
    VOID CreateRenderItems();
    VOID CreateFrameResources();
    VOID CreateDescriptorHeaps();
    VOID CreateConstantBufferViews();

    VOID PopulateCommandList();
    VOID MoveToNextFrame();
    VOID WaitForGPU();


    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV()
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV()
    {
        return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    std::vector<UINT8> GenerateTextureData();
};