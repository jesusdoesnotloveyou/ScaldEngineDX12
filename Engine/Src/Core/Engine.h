#pragma once

#include "D3D12Sample.h"
#include "FrameResource.h"
#include "Camera.h"
#include "CascadeShadowMap.h"
#include "GBuffer.h"
#include "GameFramework/Components/Scene.h"
#include "GameFramework/Objects/SObject.h"
#include "RootSignature.h"

const int gNumFrameResources = 3;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

using Microsoft::WRL::ComPtr;

struct Material
{
    Material(const char* name)
        : Name(std::string(name))
    {
    }

    Material(const char* name, int materialBufferIndex, int diffuseSrvHeapIndex)
        : Name(std::string(name))
        , MatBufferIndex(materialBufferIndex)
        , DiffuseSrvHeapIndex(diffuseSrvHeapIndex)
    {

    }

    std::string Name;

    // Index into constant/structured buffer corresponding to this material (to map with render item).
    int MatBufferIndex = -1;

    // Index into SRV heap for diffuse texture. Index of corresponding texture in Texture2D[n] 
    int DiffuseSrvHeapIndex = -1;

    // Index into SRV heap for normal texture.
    int NormalSrvHeapIndex = -1;

    int NumFramesDirty = gNumFrameResources;

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // could be used for material animation (water for instance)
    XMMATRIX MatTransform = XMMatrixIdentity();
};

// F. Luna stuff: lightweight structure that stores parameters to draw a shape.
struct RenderItem
{
    RenderItem(int objectCBIndex = -1)
        : ObjCBIndex(objectCBIndex)
    {
    }

    XMMATRIX World = XMMatrixIdentity();
    // could be used for texture tiling
    XMMATRIX TexTransform = XMMatrixIdentity();

    BoundingBox Bounds;
    std::vector<InstanceData> Instances; // for spot and point lights for now
    
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    MeshGeometry* Geo = nullptr;
    Material* Mat = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT InstanceCount = 0u;            // for spot and point lights for now
    UINT IndexCount = 0u;
    UINT StartIndexLocation = 0u;
    int BaseVertexLocation = 0;
};

class Engine : public D3D12Sample
{
public:
    enum ERootParameter : UINT
    {
        PerObjectDataCB = 0,
        PerPassDataCB,
        MaterialDataSB,
        PointLightsDataSB,
        SpotLightsDataSB,
        CascadedShadowMaps,
        Textures,
        GBufferTextures,

        NumRootParameters = 8u
    };

    enum EPsoType : UINT
    {
        CascadedShadowsOpaque = 0,
        
        DeferredGeometry,
        Wireframe,

        DeferredDirectional,
        DeferredPointWithinFrustum,
        DeferredPointIntersectsFarPlane,
        DeferredPointFullQuad,
        DeferredSpot,

        Transparency,
        
        NumPipelineStates = 9u
    };

    enum EShaderType : UINT
    {
        // Forward rendering
        DefaultVS = 0,
        DefaultOpaquePS,

        CascadedShadowsVS,
        CascadedShadowsGS,

        DeferredGeometryVS,
        DeferredGeometryPS,
        DeferredDirVS,
        DeferredDirPS,
        DeferredLightVolumesVS,
        DeferredPointPS,
        DeferredSpotPS,

        NumShaders = 11U
    };

public:
    Engine(UINT width, UINT height, const std::wstring& name, const std::wstring& className);
    virtual ~Engine() override;

    virtual void OnInit() override;
    virtual void OnUpdate(const ScaldTimer& st) override;
    virtual void OnRender(const ScaldTimer& st) override;
    virtual void OnDestroy() override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
    virtual void OnKeyDown(UINT8 key) override;
    virtual void OnKeyUp(UINT8 key) override;

private:
    void OnKeyboardInput(const ScaldTimer& st);
    void UpdateObjectsCB(const ScaldTimer& st);
    void UpdateMaterialBuffer(const ScaldTimer& st);
    void UpdateLightsBuffer(const ScaldTimer& st);
    void UpdateShadowTransform(const ScaldTimer& st);
    void UpdateShadowPassCB(const ScaldTimer& st);
    void UpdateGeometryPassCB(const ScaldTimer& st);
    void UpdateMainPassCB(const ScaldTimer& st);
    
private:
#pragma region Shadows
    void RenderDepthOnlyPass();
#pragma endregion Shadows
#pragma region DeferredShading
    void RenderGeometryPass();
    void RenderLightingPass();
    void RenderTransparencyPass();

    void DeferredDirectionalLightPass();
    void DeferredPointLightPass();
    void DeferredSpotLightPass();
#pragma endregion DeferredShading

    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems);
    void DrawInstancedRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems);
    void DrawQuad(ID3D12GraphicsCommandList* cmdList);

private:
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;
    FrameResource* m_currFrameResource = nullptr;
    int m_currFrameResourceIndex = 0;

    UINT m_passCbvOffset = 0;

    float m_sunPhi = XM_PIDIV4;
    float m_sunTheta = 1.25f * XM_PI;
    
    std::shared_ptr<RootSignature> m_rootSignature;

    ComPtr<ID3D12DescriptorHeap> m_cbvHeap; // Heap for constant buffer views
    ComPtr<ID3D12DescriptorHeap> m_srvHeap; // Heap for textures
   
    std::unordered_map<EShaderType, ComPtr<ID3DBlob>> m_shaders;
    std::unordered_map<EPsoType, ComPtr<ID3D12PipelineState>> m_pipelineStates;

    ObjectConstants m_perObjectCBData;
    PassConstants m_shadowPassCBData;
    PassConstants m_geometryPassCBData;
    PassConstants m_mainPassCBData; // deferred color(light) pass
    //PassConstants m_lightingPassCBData;
    MaterialData m_perMaterialSBData;
    InstanceData m_perInstanceSBData;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;

    std::vector<std::unique_ptr<RenderItem>> m_renderItems;
    std::vector<Scald::SObject> m_sceneObjects;
    std::vector<std::unique_ptr<RenderItem>> m_pointLights;
    std::vector<RenderItem*> m_opaqueItems;

    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<ShadowMap> m_cascadeShadowMap;
    std::shared_ptr<Scald::Scene> m_scene;

#pragma region DeferredShading
    std::unique_ptr<GBuffer> m_GBuffer;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_GBufferTexturesSrv;
#pragma endregion DeferredShading

#pragma region CascadedShadows
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_cascadeShadowSrv;
#pragma endregion CascadedShadows

private:
    VOID LoadPipeline() override;
    VOID LoadGraphicsFeatures();
    VOID LoadCSMResources();
    VOID LoadDeferredRenderingResources();
    
    VOID Reset() override;
    VOID CreateRtvAndDsvDescriptorHeaps() override;
    
    VOID LoadAssets();
    VOID CreateRootSignature();
    VOID CreateShaders();
    VOID CreatePSO();
    
    VOID LoadScene();
    VOID LoadTextures();
    // Shapes
    VOID CreateGeometry();
    // Propertirs of shapes' surfaces to model light interaction
    VOID CreateGeometryMaterials();
    // Shapes could constist of some items to render
    VOID CreateSceneObjects();
    VOID CreateRenderItems();
    VOID CreatePointLights();
    VOID CreateFrameResources();
    // Heaps are created if there are root descriptor tables in root signature 
    VOID CreateDescriptorHeaps();

    VOID PopulateCommandList();
    VOID MoveToNextFrame();

    std::pair<XMMATRIX, XMMATRIX> GetLightSpaceMatrix(const float nearPlane, const float farPlane);
    // Doubt that't a good idea to return vector of matrices. Should rather pass vector as a parameter probalby and fill it inside function.
    void GetLightSpaceMatrices(std::vector<std::pair<XMMATRIX, XMMATRIX>>& outMatrices);

    std::vector<XMVECTOR> GetFrustumCornersWorldSpace(const XMMATRIX& view, const XMMATRIX& projection);
};