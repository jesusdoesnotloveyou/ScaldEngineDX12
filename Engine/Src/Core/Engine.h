#pragma once

#include "D3D12Sample.h"
#include "FrameResource.h"
#include "Camera.h"

const int gNumFrameResources = 3;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

using Microsoft::WRL::ComPtr;

struct Material
{
    std::string Name;

    // Index into constant buffer corresponding to this material.
    int MatCBIndex = -1;

    // Index into SRV heap for diffuse texture.
    int DiffuseSrvHeapIndex = -1;

    // Index into SRV heap for normal texture.
    int NormalSrvHeapIndex = -1;

    int NumFramesDirty = gNumFrameResources;

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // could be used for material animation (water for instance)
    XMMATRIX MatTransform;
};

// F. Luna stuff: lightweight structure that stores parameters to draw a shape.
struct RenderItem
{
    RenderItem() = default;

    XMMATRIX World = XMMatrixIdentity();
    // could be used for texture tiling
    XMMATRIX TexTransform = XMMatrixIdentity();

    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    MeshGeometry* Geo = nullptr;
    Material* Mat = nullptr;

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
    void UpdateMaterialCB(const ScaldTimer& st);
    void UpdateShadowTransform(const ScaldTimer& st);
    void UpdateShadowPassCB(const ScaldTimer& st);
    
    void RenderDepthOnlyPass(ID3D12GraphicsCommandList* cmdList);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems);

private:
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;
    FrameResource* m_currFrameResource = nullptr;
    int m_currFrameResourceIndex = 0;

    UINT m_passCbvOffset = 0;

    float m_sunPhi = XM_PIDIV4;
    float m_sunTheta = 1.25f * XM_PI;

    ComPtr<ID3D12RootSignature> m_rootSignature;

    ComPtr<ID3D12DescriptorHeap> m_cbvHeap; // Heap for constant buffer views
    ComPtr<ID3D12DescriptorHeap> m_srvHeap; // Heap for textures
   
    std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_pipelineStates;

    ObjectConstants m_perObjectConstantBufferData;
    PassConstants m_passConstantBufferData;
    MaterialConstants m_perMaterialConstantBufferData;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
    std::unordered_map < std::string, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    std::vector<std::unique_ptr<RenderItem>> m_renderItems;
    std::vector<RenderItem*> m_opaqueItems;

    std::unique_ptr<Camera> m_camera;

    VOID LoadPipeline() override;
    VOID Reset() override;
    
    VOID LoadAssets();
    VOID CreateRootSignature();
    VOID CreateShaders();
    VOID CreatePSO();
    
    VOID LoadTextures();
    // Shapes
    VOID CreateGeometry();
    // Propertirs of shapes' surfaces to model light interaction
    VOID CreateGeometryMaterials();
    // Shapes could constist of some items to render
    VOID CreateRenderItems();
    VOID CreateFrameResources();
    // Heaps are created if there are root descriptor tables in root signature 
    VOID CreateDescriptorHeaps();
    VOID CreateConstantBufferViews();

    VOID PopulateCommandList();
    VOID MoveToNextFrame();

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> Engine::GetStaticSamplers();
};