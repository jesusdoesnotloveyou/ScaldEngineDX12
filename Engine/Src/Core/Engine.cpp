#include "stdafx.h"
#include "Engine.h"
#include "Common/ScaldMath.h"
#include "GameFramework/Components/Scene.h"
#include "GameFramework/Components/Transform.h"
#include "GameFramework/Components/Renderer.h"
#include "CommandQueue.h"

extern const int gNumFrameResources;

Engine::Engine(UINT width, UINT height, const std::wstring& name, const std::wstring& className)
    : 
    Super(width, height, name, className)
{
    m_camera = std::make_unique<Camera>();
}

Engine::~Engine()
{
    if (m_device)
    {
        m_commandQueue->Flush();
    }
}

void Engine::OnInit()
{
    LoadPipeline();

    LoadGraphicsFeatures();

    LoadAssets();
}

// Load the rendering pipeline dependencies.
VOID Engine::LoadPipeline()
{
    Super::LoadPipeline();
}

VOID Engine::LoadGraphicsFeatures()
{
    LoadCSMResources();
    LoadDeferredRenderingResources();
}

VOID Engine::LoadCSMResources()
{
    m_cascadeShadowMap = std::make_unique<CascadeShadowMap>(m_device.Get(), 2048u, 2048u, MaxCascades);
    m_cascadeShadowMap->CreateShadowCascadeSplits(m_camera->GetNearZ(), m_camera->GetFarZ());
}

VOID Engine::LoadDeferredRenderingResources()
{
    m_GBuffer = std::make_unique<GBuffer>(m_device.Get(), m_width, m_height);
}

// Load the sample assets.
VOID Engine::LoadAssets()
{
    auto commandList = m_commandQueue->GetCommandList(m_commandAllocator.Get()); 
    //ThrowIfFailed(commandList->Reset(m_commandAllocator.Get(), nullptr));
    
    LoadScene();
    LoadTextures(commandList.Get());
    CreateGeometry(commandList.Get());
    CreateGeometryMaterials();
    CreateRenderItems();
    CreatePointLights(commandList.Get());
    CreateFrameResources();
    CreateDescriptorHeaps();
    CreateRootSignature();
    CreateShaders();
    CreatePSO();

    m_commandQueue->ExecuteCommandList(commandList);
}

VOID Engine::CreateRootSignature()
{
    m_rootSignature = std::make_shared<RootSignature>();

    CD3DX12_DESCRIPTOR_RANGE cascadeShadowSrv;
    cascadeShadowSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u);
    
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, /* number of textures2D */(UINT)m_textures.size(), 2u, 0u);

    CD3DX12_DESCRIPTOR_RANGE gBufferTable;
    gBufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, (UINT)GBuffer::EGBufferLayer::MAX, 2u, 1u);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[ERootParameter::NumRootParameters];
    
    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[ERootParameter::PerObjectDataCB].InitAsConstantBufferView(0u, 0u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialIndex used in both shaders */); // a root descriptor for objects' CBVs.
    slotRootParameter[ERootParameter::PerPassDataCB].InitAsConstantBufferView(1u, 0u, D3D12_SHADER_VISIBILITY_ALL);                                             // a root descriptor for Pass CBV.
    slotRootParameter[ERootParameter::MaterialDataSB].InitAsShaderResourceView(1u, 0u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialData used in both shaders */);   // a srv for structured buffer with materials' data
    slotRootParameter[ERootParameter::PointLightsDataSB].InitAsShaderResourceView(0u, 1u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialData used in both shaders */);   // a srv for structured buffer with materials' data
    slotRootParameter[ERootParameter::SpotLightsDataSB].InitAsShaderResourceView(1u, 1u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialData used in both shaders */);   // a srv for structured buffer with materials' data
    slotRootParameter[ERootParameter::CascadedShadowMaps].InitAsDescriptorTable(1u, &cascadeShadowSrv, D3D12_SHADER_VISIBILITY_PIXEL);                          // a descriptor table for shadow maps array.
    slotRootParameter[ERootParameter::Textures].InitAsDescriptorTable(1u, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);                                            // a descriptor table for textures
    slotRootParameter[ERootParameter::GBufferTextures].InitAsDescriptorTable(1u, &gBufferTable, D3D12_SHADER_VISIBILITY_PIXEL);                                 // a descriptor table for GBuffer

    m_rootSignature->Create(m_device.Get(), ARRAYSIZE(slotRootParameter), slotRootParameter, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

VOID Engine::CreateShaders()
{
    //auto pixelShaderPath = GetAssetFullPath(L"./PixelShader.hlsl").c_str();

    const D3D_SHADER_MACRO fogDefines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO shadowDebugDefines[] =
    {
        "SHADOW_DEBUG", "1",
        NULL, NULL
    };

    m_shaders[EShaderType::DefaultVS] = ScaldUtil::CompileShader(L"./Assets/Shaders/VertexShader.hlsl", nullptr, "main", "vs_5_1");
    m_shaders[EShaderType::DefaultOpaquePS] = ScaldUtil::CompileShader(L"./Assets/Shaders/PixelShader.hlsl", fogDefines, "main", "ps_5_1");

    m_shaders[EShaderType::CascadedShadowsVS] = ScaldUtil::CompileShader(L"./Assets/Shaders/ShadowVertexShader.hlsl", nullptr, "main", "vs_5_1");
    m_shaders[EShaderType::CascadedShadowsGS] = ScaldUtil::CompileShader(L"./Assets/Shaders/GeometryShader.hlsl", nullptr, "main", "gs_5_1");

#pragma region DeferredShading
    m_shaders[EShaderType::DeferredGeometryVS] = ScaldUtil::CompileShader(L"./Assets/Shaders/GBufferPassVS.hlsl", nullptr, "main", "vs_5_1");
    m_shaders[EShaderType::DeferredGeometryPS] = ScaldUtil::CompileShader(L"./Assets/Shaders/GBufferPassPS.hlsl", nullptr, "main", "ps_5_1");

    m_shaders[EShaderType::DeferredDirVS] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredDirectionalLightVS.hlsl", nullptr, "main", "vs_5_1");
    m_shaders[EShaderType::DeferredDirPS] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredDirectionalLightPS.hlsl", nullptr, "main", "ps_5_1");

    m_shaders[EShaderType::DeferredLightVolumesVS] = ScaldUtil::CompileShader(L"./Assets/Shaders/LightVolumesVS.hlsl", nullptr, "main", "vs_5_1");
    m_shaders[EShaderType::DeferredPointPS] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredPointLightPS.hlsl", nullptr, "main", "ps_5_1");
    m_shaders[EShaderType::DeferredSpotPS] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredSpotLightPS.hlsl", nullptr, "main", "ps_5_1");
#pragma endregion DeferredShading
}

VOID Engine::CreatePSO()
{
    // To hold common properties
    D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc = {};
    defaultPsoDesc.pRootSignature = m_rootSignature->Get();
    defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // Blend state is disable
    defaultPsoDesc.SampleMask = UINT_MAX;
    defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    defaultPsoDesc.InputLayout = VertexPositionNormalTangentUV::InputLayout;
    defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    defaultPsoDesc.NumRenderTargets = 1u;
    defaultPsoDesc.RTVFormats[0] = BackBufferFormat;
    defaultPsoDesc.DSVFormat = DepthStencilFormat;
    defaultPsoDesc.SampleDesc = { 1u, 0u }; // No MSAA. This should match the setting of the render target we are using (check swapChainDesc)

#pragma region CascadeShadowsDepthPass
    D3D12_GRAPHICS_PIPELINE_STATE_DESC cascadeShadowPsoDesc = defaultPsoDesc;
    cascadeShadowPsoDesc.pRootSignature = m_rootSignature->Get();
    cascadeShadowPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::CascadedShadowsVS)->GetBufferPointer()),
                m_shaders.at(EShaderType::CascadedShadowsVS)->GetBufferSize()
        });
    cascadeShadowPsoDesc.GS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::CascadedShadowsGS)->GetBufferPointer()),
                m_shaders.at(EShaderType::CascadedShadowsGS)->GetBufferSize()
        });
    cascadeShadowPsoDesc.RasterizerState.DepthBias = 10000;
    cascadeShadowPsoDesc.RasterizerState.DepthClipEnable = (BOOL)0.0f;
    cascadeShadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    cascadeShadowPsoDesc.InputLayout = VertexPosition::InputLayout;
    cascadeShadowPsoDesc.NumRenderTargets = 0u;
    cascadeShadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&cascadeShadowPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::CascadedShadowsOpaque])));
#pragma endregion CascadeShadowsDepthPass

#pragma region DeferredShading
    D3D12_GRAPHICS_PIPELINE_STATE_DESC GBufferPsoDesc = defaultPsoDesc;
    GBufferPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredGeometryVS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredGeometryVS)->GetBufferSize()
        });
    GBufferPsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredGeometryPS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredGeometryPS)->GetBufferSize()
        });
    GBufferPsoDesc.NumRenderTargets = static_cast<UINT>(GBuffer::EGBufferLayer::MAX) - 1u;
    GBufferPsoDesc.RTVFormats[0] = m_GBuffer->GetBufferTextureFormat(GBuffer::EGBufferLayer::DIFFUSE_ALBEDO);
    GBufferPsoDesc.RTVFormats[1] = m_GBuffer->GetBufferTextureFormat(GBuffer::EGBufferLayer::AMBIENT_OCCLUSION);
    GBufferPsoDesc.RTVFormats[2] = m_GBuffer->GetBufferTextureFormat(GBuffer::EGBufferLayer::NORMAL);
    GBufferPsoDesc.RTVFormats[3] = m_GBuffer->GetBufferTextureFormat(GBuffer::EGBufferLayer::SPECULAR);
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&GBufferPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::DeferredGeometry])));

#pragma region DeferredDirectional
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dirLightPsoDesc = defaultPsoDesc;
    dirLightPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredDirVS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredDirVS)->GetBufferSize()
        });
    dirLightPsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredDirPS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredDirPS)->GetBufferSize()
        });
    dirLightPsoDesc.InputLayout = VertexPosition::InputLayout;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dirLightPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::DeferredDirectional])));
#pragma endregion DeferredDirectional

    // not sure it works
#pragma region Wireframe
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframe = GBufferPsoDesc;
    opaqueWireframe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaqueWireframe, IID_PPV_ARGS(&m_pipelineStates[EPsoType::Wireframe])));
#pragma endregion Wireframe

#pragma region DeferredPointLight
    // Still needs to be configured
    // Hack with DSS and RS
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pointLightIntersectsFarPlanePsoDesc = dirLightPsoDesc;
    
    D3D12_RENDER_TARGET_BLEND_DESC RTBlendDesc = {};
    ZeroMemory(&RTBlendDesc, sizeof(D3D12_RENDER_TARGET_BLEND_DESC));
    RTBlendDesc.BlendEnable = TRUE;
    RTBlendDesc.LogicOpEnable = FALSE;
    RTBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    RTBlendDesc.DestBlend = D3D12_BLEND_ONE;
    RTBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    RTBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    RTBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
    RTBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    RTBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    RTBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pointLightIntersectsFarPlanePsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredLightVolumesVS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredLightVolumesVS)->GetBufferSize()
        });
    pointLightIntersectsFarPlanePsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredPointPS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredPointPS)->GetBufferSize()
        });
    pointLightIntersectsFarPlanePsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    pointLightIntersectsFarPlanePsoDesc.BlendState.IndependentBlendEnable = FALSE;
    pointLightIntersectsFarPlanePsoDesc.BlendState.RenderTarget[0] = RTBlendDesc;

    pointLightIntersectsFarPlanePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.DepthEnable = FALSE;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.StencilEnable = FALSE;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.StencilReadMask = 0xFF;
    pointLightIntersectsFarPlanePsoDesc.DepthStencilState.StencilWriteMask = 0xFF;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pointLightIntersectsFarPlanePsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::DeferredPointIntersectsFarPlane])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pointLightWithinFrustumPsoDesc = pointLightIntersectsFarPlanePsoDesc;
    pointLightWithinFrustumPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    pointLightWithinFrustumPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pointLightWithinFrustumPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::DeferredPointWithinFrustum])));

#pragma endregion DeferredPointLight

#pragma region DeferredSpotLight
    D3D12_GRAPHICS_PIPELINE_STATE_DESC spotLightPsoDesc = pointLightIntersectsFarPlanePsoDesc;
    spotLightPsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DeferredSpotPS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DeferredSpotPS)->GetBufferSize()
        });
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&spotLightPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::DeferredSpot])));
#pragma endregion DeferredSpotLight

    // Transparent objects are drawn in forward rendering style
#pragma region Transparency
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = defaultPsoDesc;
    transparentPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DefaultVS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DefaultVS)->GetBufferSize()
        });
    transparentPsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at(EShaderType::DefaultOpaquePS)->GetBufferPointer()),
            m_shaders.at(EShaderType::DefaultOpaquePS)->GetBufferSize()
        });

    // C = C(src) * F(src) + C(dst) * F(dst)
    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc = {};
    ZeroMemory(&transparencyBlendDesc, sizeof(transparencyBlendDesc));
    transparencyBlendDesc.BlendEnable = TRUE;
    transparencyBlendDesc.LogicOpEnable = FALSE;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    transparentPsoDesc.BlendState.IndependentBlendEnable = FALSE;
    transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    // since we can see through transparent objects, we have to see their back faces
    transparentPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&m_pipelineStates[EPsoType::Transparency])));
#pragma endregion Transparency
#pragma endregion DeferredShading
}

VOID Engine::LoadScene()
{
    m_scene = std::make_shared<Scald::Scene>();
}

VOID Engine::LoadTextures(ID3D12GraphicsCommandList* pCommandList)
{
    auto brickTex = std::make_unique<Texture>("brickTex", L"./Assets/Textures/bricks.dds", m_device.Get(), pCommandList);
    auto grassTex = std::make_unique<Texture>("grassTex", L"./Assets/Textures/grass.dds", m_device.Get(), pCommandList);
    auto iceTex = std::make_unique<Texture>("iceTex", L"./Assets/Textures/ice.dds", m_device.Get(), pCommandList);
    auto stoneTex = std::make_unique<Texture>("stoneTex", L"./Assets/Textures/stone.dds", m_device.Get(), pCommandList);
    auto tileTex = std::make_unique<Texture>("tileTex", L"./Assets/Textures/tile.dds", m_device.Get(), pCommandList);
    auto planksTex = std::make_unique<Texture>("planksTex", L"./Assets/Textures/planks.dds", m_device.Get(), pCommandList);

    m_textures[brickTex->Name] = std::move(brickTex);
    m_textures[grassTex->Name] = std::move(grassTex);
    m_textures[iceTex->Name] = std::move(iceTex);
    m_textures[stoneTex->Name] = std::move(stoneTex);
    m_textures[tileTex->Name] = std::move(tileTex);
    m_textures[planksTex->Name] = std::move(planksTex);
}

VOID Engine::CreateGeometry(ID3D12GraphicsCommandList* pCommandList)
{
    auto sphereMesh = m_scene->GetBuiltInMesh(Scald::EBuiltInMeshes::SPHERE);
    auto gridMesh = m_scene->GetBuiltInMesh(Scald::EBuiltInMeshes::GRID);

    // Create shared vertex/index buffer for all geometry.
    UINT sunVertexOffset = 0u;
    UINT mercuryVertexOffset = sunVertexOffset + (UINT)sphereMesh.LODVertices[0].size();
    UINT venusVertexOffset = mercuryVertexOffset + (UINT)sphereMesh.LODVertices[0].size();
    UINT earthVertexOffset = venusVertexOffset + (UINT)sphereMesh.LODVertices[0].size();
    UINT marsVertexOffset = earthVertexOffset + (UINT)sphereMesh.LODVertices[0].size();
    UINT planeVertexOffset = marsVertexOffset + (UINT)sphereMesh.LODVertices[0].size();

    UINT sunIndexOffset = 0u;
    UINT mercuryIndexOffset = sunIndexOffset + (UINT)sphereMesh.LODIndices[0].size();
    UINT venusIndexOffset = mercuryIndexOffset + (UINT)sphereMesh.LODIndices[0].size();
    UINT earthIndexOffset = venusIndexOffset + (UINT)sphereMesh.LODIndices[0].size();
    UINT marsIndexOffset = earthIndexOffset + (UINT)sphereMesh.LODIndices[0].size();
    UINT planeIndexOffset = marsIndexOffset + (UINT)sphereMesh.LODIndices[0].size();

    auto createSubmeshWithParams = [](const MeshData<>& mesh, const UINT indexOffset, const UINT vertexOffset) -> SubmeshGeometry
        {
            SubmeshGeometry submesh;
            submesh.IndexCount = (UINT)mesh.LODIndices[0].size();
            submesh.StartIndexLocation = indexOffset;
            submesh.BaseVertexLocation = vertexOffset;
            submesh.Bounds = mesh.LODBounds[0];
            return submesh;
        };

    auto totalVertexCount = 
        sphereMesh.LODVertices[0].size() // sun
        + sphereMesh.LODVertices[0].size() // merc
        + sphereMesh.LODVertices[0].size() // venus
        + sphereMesh.LODVertices[0].size() //earth
        + sphereMesh.LODVertices[0].size() //mars
        + gridMesh.LODVertices[0].size() //mars
        ;
    auto totalIndexCount = 
        sphereMesh.LODIndices[0].size()
        + sphereMesh.LODIndices[0].size()
        + sphereMesh.LODIndices[0].size()
        + sphereMesh.LODIndices[0].size()
        + sphereMesh.LODIndices[0].size()
        + gridMesh.LODIndices[0].size()
        ;

    std::vector<VertexPositionNormalTangentUV> vertices(totalVertexCount);

    int k = 0;
    for (size_t i = 0; i < sphereMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = sphereMesh.LODVertices[0][i].position;
        vertices[k].normal = sphereMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = sphereMesh.LODVertices[0][i].texCoord;
    }

    for (size_t i = 0; i < sphereMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = sphereMesh.LODVertices[0][i].position;
        vertices[k].normal = sphereMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = sphereMesh.LODVertices[0][i].texCoord;
    }

    for (size_t i = 0; i < sphereMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = sphereMesh.LODVertices[0][i].position;
        vertices[k].normal = sphereMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = sphereMesh.LODVertices[0][i].texCoord;
    }

    for (size_t i = 0; i < sphereMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = sphereMesh.LODVertices[0][i].position;
        vertices[k].normal = sphereMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = sphereMesh.LODVertices[0][i].texCoord;
    }

    for (size_t i = 0; i < sphereMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = sphereMesh.LODVertices[0][i].position;
        vertices[k].normal = sphereMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = sphereMesh.LODVertices[0][i].texCoord;
    }

    for (size_t i = 0; i < gridMesh.LODVertices[0].size(); ++i, ++k)
    {
        vertices[k].position = gridMesh.LODVertices[0][i].position;
        vertices[k].normal = gridMesh.LODVertices[0][i].normal;
        vertices[k].texCoord = gridMesh.LODVertices[0][i].texCoord;
    }

    std::vector<uint16_t> indices;
    indices.insert(indices.end(), sphereMesh.LODIndices[0].begin(), sphereMesh.LODIndices[0].end());
    indices.insert(indices.end(), sphereMesh.LODIndices[0].begin(), sphereMesh.LODIndices[0].end());
    indices.insert(indices.end(), sphereMesh.LODIndices[0].begin(), sphereMesh.LODIndices[0].end());
    indices.insert(indices.end(), sphereMesh.LODIndices[0].begin(), sphereMesh.LODIndices[0].end());
    indices.insert(indices.end(), sphereMesh.LODIndices[0].begin(), sphereMesh.LODIndices[0].end());
    indices.insert(indices.end(), gridMesh.LODIndices[0].begin(), gridMesh.LODIndices[0].end());

    auto solarSystem = std::make_unique<MeshGeometry>("solarSystem");

    solarSystem->DrawArgs["sun"] = createSubmeshWithParams(sphereMesh, sunIndexOffset, sunVertexOffset);
    solarSystem->DrawArgs["mercury"] = createSubmeshWithParams(sphereMesh, mercuryIndexOffset, mercuryVertexOffset);
    solarSystem->DrawArgs["venus"] = createSubmeshWithParams(sphereMesh, venusIndexOffset, venusVertexOffset);
    solarSystem->DrawArgs["earth"] = createSubmeshWithParams(sphereMesh, earthIndexOffset, earthVertexOffset);
    solarSystem->DrawArgs["mars"] = createSubmeshWithParams(sphereMesh, marsIndexOffset, marsVertexOffset);
    solarSystem->DrawArgs["plane"] = createSubmeshWithParams(gridMesh, planeIndexOffset, planeVertexOffset);

    auto fullQuad = std::make_unique<MeshGeometry>("fullQuad");
    std::vector<VertexPosition> fullQuadVertices(4, VertexPosition{}); // SV_VertexID in DeferredDirLightVS
                                                                       // we don't need any other data besides vertex position,
                                                                       // so there is actually no input layout for that shader
    solarSystem->CreateGPUBuffers(m_device.Get(), pCommandList, vertices, indices);
    fullQuad->CreateGPUBuffers(m_device.Get(), pCommandList, fullQuadVertices);

    m_geometries[solarSystem->Name] = std::move(solarSystem);
    m_geometries[fullQuad->Name] = std::move(fullQuad);
}

VOID Engine::CreateGeometryMaterials()
{
    // Should probably be global scene variables
    int MaterialBufferIndex = 0;
    int DiffuseSrvHeapIndex = 0;

    // DiffuseAlbedo in materials is set (1,1,1,1) by default to not affect texture diffuse albedo
    auto flame0 = std::make_unique<Material>("flame0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    //flame0->DiffuseAlbedo = XMFLOAT4(Colors::Gold);
    flame0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    flame0->Roughness = 0.2f;
    flame0->MatTransform = XMMatrixIdentity();

    auto sand0 = std::make_unique<Material>("sand0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    //sand0->DiffuseAlbedo = XMFLOAT4(Colors::Brown);
    sand0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    sand0->Roughness = 0.1f;
    sand0->MatTransform = XMMatrixIdentity();

    auto stone0 = std::make_unique<Material>("stone0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    //stone0->DiffuseAlbedo = XMFLOAT4(Colors::Orchid);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;
    stone0->MatTransform = XMMatrixIdentity();

    auto ground0 = std::make_unique<Material>("ground0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    //ground0->DiffuseAlbedo = XMFLOAT4(Colors::Green);
    ground0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    ground0->Roughness = 0.3f;
    ground0->MatTransform = XMMatrixIdentity();

    auto wood0 = std::make_unique<Material>("wood0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    wood0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    wood0->Roughness = 0.5f;
    wood0->MatTransform = XMMatrixIdentity();

    auto iron0 = std::make_unique<Material>("iron0", MaterialBufferIndex++, DiffuseSrvHeapIndex++);
    iron0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    iron0->Roughness = 0.1f;
    iron0->MatTransform = XMMatrixIdentity();

    m_materials["flame0"] = std::move(flame0);
    m_materials["sand0"] = std::move(sand0);
    m_materials["stone0"] = std::move(stone0);
    m_materials["ground0"] = std::move(ground0);
    m_materials["iron0"] = std::move(iron0);
    m_materials["wood0"] = std::move(wood0);
}

VOID Engine::CreateSceneObjects()
{
    auto testObj = std::make_shared<Scald::SObject>();
    testObj->AddComponent<Scald::Transform>(ScaldMath::ZeroVector, ScaldMath::ZeroVector, ScaldMath::One);
    testObj->AddComponent<Scald::Renderer>();
}

VOID Engine::CreateRenderItems()
{
    // Should probably be global scene variable or incapsulated inside scene class
    int ObjectCBIndex = 0;

    auto sunRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    sunRenderItem->World = XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    sunRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    sunRenderItem->Geo = m_geometries.at("solarSystem").get();
    sunRenderItem->Mat = m_materials.at("flame0").get();
    sunRenderItem->IndexCount = sunRenderItem->Geo->DrawArgs.at("sun").IndexCount;
    sunRenderItem->StartIndexLocation = sunRenderItem->Geo->DrawArgs.at("sun").StartIndexLocation;
    sunRenderItem->BaseVertexLocation = sunRenderItem->Geo->DrawArgs.at("sun").BaseVertexLocation;
    sunRenderItem->Bounds = sunRenderItem->Geo->DrawArgs.at("sun").Bounds;

    auto mercuryRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    mercuryRenderItem->World = XMMatrixScaling(0.25f, 0.25f, 0.25f) * XMMatrixTranslation(2.0f, 0.0f, 0.0f);
    mercuryRenderItem->Geo = m_geometries.at("solarSystem").get();
    mercuryRenderItem->Mat = m_materials.at("sand0").get();
    mercuryRenderItem->IndexCount = mercuryRenderItem->Geo->DrawArgs.at("mercury").IndexCount;
    mercuryRenderItem->StartIndexLocation = mercuryRenderItem->Geo->DrawArgs.at("mercury").StartIndexLocation;
    mercuryRenderItem->BaseVertexLocation = mercuryRenderItem->Geo->DrawArgs.at("mercury").BaseVertexLocation;
    mercuryRenderItem->Bounds = mercuryRenderItem->Geo->DrawArgs.at("mercury").Bounds;

    auto venusRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    venusRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(3.0f, 0.0f, 3.0f);
    venusRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    venusRenderItem->Geo = m_geometries.at("solarSystem").get();
    venusRenderItem->Mat = m_materials.at("stone0").get();
    venusRenderItem->IndexCount = venusRenderItem->Geo->DrawArgs.at("venus").IndexCount;
    venusRenderItem->StartIndexLocation = venusRenderItem->Geo->DrawArgs.at("venus").StartIndexLocation;
    venusRenderItem->BaseVertexLocation = venusRenderItem->Geo->DrawArgs.at("venus").BaseVertexLocation;
    venusRenderItem->Bounds = venusRenderItem->Geo->DrawArgs.at("venus").Bounds;

    auto earthRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    earthRenderItem->World = XMMatrixScaling(0.6f, 0.6f, 0.6f) * XMMatrixTranslation(4.0f, 0.0f, 4.0f);
    earthRenderItem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
    earthRenderItem->Geo = m_geometries.at("solarSystem").get();
    earthRenderItem->Mat = m_materials.at("ground0").get();
    earthRenderItem->IndexCount = earthRenderItem->Geo->DrawArgs.at("earth").IndexCount;
    earthRenderItem->StartIndexLocation = earthRenderItem->Geo->DrawArgs.at("earth").StartIndexLocation;
    earthRenderItem->BaseVertexLocation = earthRenderItem->Geo->DrawArgs.at("earth").BaseVertexLocation;
    earthRenderItem->Bounds = earthRenderItem->Geo->DrawArgs.at("earth").Bounds;

    auto marsRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    marsRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(5.0f, 0.0f, 5.0f);
    marsRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    marsRenderItem->Geo = m_geometries.at("solarSystem").get();
    marsRenderItem->Mat = m_materials.at("iron0").get();
    marsRenderItem->IndexCount = marsRenderItem->Geo->DrawArgs.at("mars").IndexCount;
    marsRenderItem->StartIndexLocation = marsRenderItem->Geo->DrawArgs.at("mars").StartIndexLocation;
    marsRenderItem->BaseVertexLocation = marsRenderItem->Geo->DrawArgs.at("mars").BaseVertexLocation;
    marsRenderItem->Bounds = marsRenderItem->Geo->DrawArgs.at("mars").Bounds;

    auto planeRenderItem = std::make_unique<RenderItem>(ObjectCBIndex++);
    planeRenderItem->World = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, -1.5f, 0.0f);
    planeRenderItem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
    planeRenderItem->Geo = m_geometries.at("solarSystem").get();
    planeRenderItem->Mat = m_materials.at("wood0").get();
    planeRenderItem->IndexCount = planeRenderItem->Geo->DrawArgs.at("plane").IndexCount;
    planeRenderItem->StartIndexLocation = planeRenderItem->Geo->DrawArgs.at("plane").StartIndexLocation;
    planeRenderItem->BaseVertexLocation = planeRenderItem->Geo->DrawArgs.at("plane").BaseVertexLocation;

    m_renderItems.push_back(std::move(sunRenderItem));
    m_renderItems.push_back(std::move(mercuryRenderItem));
    m_renderItems.push_back(std::move(venusRenderItem));
    m_renderItems.push_back(std::move(earthRenderItem));
    m_renderItems.push_back(std::move(marsRenderItem));
    m_renderItems.push_back(std::move(planeRenderItem));

    for (auto& ri : m_renderItems)
    {
        m_opaqueItems.push_back(ri.get());
    }
}

VOID Engine::CreatePointLights(ID3D12GraphicsCommandList* pCommandList)
{
    // create point light mesh as sphere, since there is issue with geosphere mesh (see in renderDoc)
    MeshData sphereMesh = m_scene->GetBuiltInMesh(Scald::EBuiltInMeshes::SPHERE);

    auto pointLightMesh = std::make_unique<MeshGeometry>("pointLightMesh");
    pointLightMesh->CreateGPUBuffers(m_device.Get(), pCommandList, sphereMesh.LODVertices[0], sphereMesh.LODIndices[0]);
    m_geometries[pointLightMesh->Name] = std::move(pointLightMesh);

    const int n = 10;

    auto pointLight = std::make_unique<RenderItem>();
    pointLight->Instances.resize(n * n);
    pointLight->World = XMMatrixIdentity();
    pointLight->Geo = m_geometries.at("pointLightMesh").get();
    pointLight->StartIndexLocation = 0u;
    pointLight->BaseVertexLocation = 0;
    pointLight->IndexCount = (UINT)sphereMesh.LODIndices[0].size();

    float width = 50.0f;
    float depth = 50.0f;

    float x = -0.5f * width;
    float z = -0.5f * depth;
    float dx = width / (n - 1);
    float dz = depth / (n - 1);

    for (int k = 0; k < n; ++k)
    {
        for (int j = 0; j < n; ++j)
        {
            int index = k*n + j;
            
            const float lightRange = ScaldMath::RandF(2.5, 3.0f);
            XMVECTOR pos = XMVectorSet(x + j * dx, -0.5f, z + k * dz, 1.0f);
            // scale should be dependent from range of light source
            XMMATRIX world = XMMatrixScalingFromVector(XMVectorReplicate(lightRange)) * XMMatrixTranslationFromVector(pos);

            XMStoreFloat4x4(&pointLight->Instances[index].World, world);

            XMStoreFloat3(&pointLight->Instances[index].Light.Position, pos);
            pointLight->Instances[index].Light.FallOfStart = ScaldMath::RandF(1.0, 2.0f);
            pointLight->Instances[index].Light.FallOfEnd = lightRange;
            pointLight->Instances[index].Light.Strength = { ScaldMath::RandF(0.0f, 1.0f), ScaldMath::RandF(0.0f, 1.0f), ScaldMath::RandF(0.0f, 1.0f) };
        }
    }

    m_pointLights.push_back(std::move(pointLight));
}

VOID Engine::CreateFrameResources()
{
    for (int i = 0; i < gNumFrameResources; i++)
    {
        m_frameResources.push_back(std::make_unique<FrameResource>(
            m_device.Get(), 
            static_cast<UINT>(EPassType::NumPasses), 
            (UINT)m_renderItems.size(), (UINT)m_materials.size(), MaxPointLights));
    }
}

VOID Engine::CreateDescriptorHeaps()
{
#if 0
#pragma region CBV
    UINT sceneObjCount = (UINT)m_renderItems.size();

    // All cbv descriptor for each object for each frame resource + descriptors for render pass cbv (gNumFrameResources because for each frame resource)
    UINT descriptorsCount = sceneObjCount * gNumFrameResources + gNumFrameResources;
    
    m_passCbvOffset = sceneObjCount * gNumFrameResources;

    // Create descriptor heaps.
    // Descriptor heap has to be created for every GPU resource
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.NumDescriptors = descriptorsCount;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
#pragma endregion CBV
#endif

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    ZeroMemory(&srvHeapDesc, sizeof(srvHeapDesc));
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                                            // textures + csm + GBuffer
    srvHeapDesc.NumDescriptors = (UINT)m_textures.size() + 1u + 5u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())));

    m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    ZeroMemory(&srvDesc, sizeof(srvDesc));

    for (auto& e : m_textures)
    {
        auto& texD3DResource = e.second->Resource;
        srvDesc.Format = texD3DResource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0u;
        srvDesc.Texture2D.MipLevels = texD3DResource->GetDesc().MipLevels;
        srvDesc.Texture2D.PlaneSlice;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        m_device->CreateShaderResourceView(texD3DResource.Get(), &srvDesc, handle);

        handle.Offset(1, m_cbvSrvUavDescriptorSize);
    }

    auto m_cascadeShadowMapHeapIndex = (UINT)m_textures.size();

    // configuring srv for shadow maps texture2Darray in the srv heap
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Texture2DArray.MostDetailedMip = 0u;
    srvDesc.Texture2DArray.MipLevels = -1;
    srvDesc.Texture2DArray.FirstArraySlice = 0u;
    srvDesc.Texture2DArray.ArraySize = m_cascadeShadowMap->Get()->GetDesc().DepthOrArraySize;
    srvDesc.Texture2DArray.PlaneSlice = 0u;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
    m_device->CreateShaderResourceView(nullptr, &srvDesc, handle);

    handle.Offset(1, m_cbvSrvUavDescriptorSize);

    auto srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto dsvCpuStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    auto rtvCpuStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    m_cascadeShadowSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize);
    m_cascadeShadowMap->CreateDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize),
        CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, m_dsvDescriptorSize));

    // to offset from csm handle to next free
    auto GBufferHeapIndex = ++m_cascadeShadowMapHeapIndex;
    m_GBufferTexturesSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, GBufferHeapIndex, m_cbvSrvUavDescriptorSize);

    for (auto i = 0u; i < GBuffer::EGBufferLayer::MAX; ++i)
    {
        srvDesc.Format = (i == GBuffer::EGBufferLayer::DEPTH) ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : m_GBuffer->GetBufferTextureFormat(i);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_device->CreateShaderResourceView(nullptr, &srvDesc, handle);
        
        auto cpuDsvRtvHandle = (i == GBuffer::EGBufferLayer::DEPTH) ? CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 2, m_dsvDescriptorSize)
            : CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, SwapChainFrameCount + i, m_rtvDescriptorSize);

        m_GBuffer->SetDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, GBufferHeapIndex, m_cbvSrvUavDescriptorSize),
            CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, GBufferHeapIndex, m_cbvSrvUavDescriptorSize),
            cpuDsvRtvHandle, i);
        
        handle.Offset(1, m_cbvSrvUavDescriptorSize);
        ++GBufferHeapIndex;
    }

    m_GBuffer->CreateDescriptors();
}

VOID Engine::Reset()
{
    Super::Reset();

    // Init/Reinit camera
    m_camera->Reset(75.0f, m_aspectRatio, 1.0f, 250.0f);
    // need tests
    //m_cascadeShadowMap->OnResize(m_width, m_height);
    //m_GBuffer->OnResize(m_width, m_height);
}

VOID Engine::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    // swap chain frames + GBuffer rtvs
    rtvHeapDesc.NumDescriptors = SwapChainFrameCount + GBuffer::EGBufferLayer::MAX - 1u;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 1 dsv + 1 cascade shadow map + 1 gbuffer depth
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 3u;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// Update frame-based values.
void Engine::OnUpdate(const ScaldTimer& st)
{
    Super::OnUpdate(st);

    OnKeyboardInput(st);
    m_camera->Update(st.DeltaTime());
    
    // Cycle through the circular frame resource array.
    m_ñurrFrameResourceIndex = (m_ñurrFrameResourceIndex + 1u) % gNumFrameResources;
    m_currFrameResource = m_frameResources[m_ñurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (m_currFrameResource->Fence != 0 /*&& !m_commandQueue->IsFenceComplete(m_currFrameResource->Fence)*/)
    {
        m_commandQueue->WaitForFenceValue(m_currFrameResource->Fence);
    }

    UpdateObjectsCB(st);
    UpdateMaterialBuffer(st);
    UpdateLightsBuffer(st);
    
    UpdateShadowTransform(st);
    UpdateShadowPassCB(st); // pass
    
    UpdateGeometryPassCB(st); // pass
    UpdateMainPassCB(st); // pass
}

// Render the scene.
void Engine::OnRender(const ScaldTimer& st)
{
    auto currCmdAlloc = m_currFrameResource->commandAllocator.Get();
    
#if defined(DEBUG) || defined(_DEBUG)
    wchar_t name[32] = {};
    UINT size = sizeof(name);
    currCmdAlloc->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
#endif

    auto commandList = m_commandQueue->GetCommandList(currCmdAlloc);
    // We could get new created command list, so it needs to be closed before reset
    commandList->Close();
    // Command list allocators can only be reset when the associated command lists have finished execution on the GPU;
    // apps should use fences to determine GPU execution progress.
    ThrowIfFailed(commandList->Reset(currCmdAlloc, nullptr));
    //
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList(commandList.Get());

    // Execute the command list.
    m_commandQueue->ExecuteCommandList(commandList);

    Present();

    m_currBackBuffer = (m_currBackBuffer + 1u) % SwapChainFrameCount;

    // Advance the fence value to mark commands up to this fence point.
    m_currFrameResource->Fence = m_commandQueue->Signal();
}

void Engine::OnDestroy()
{
    m_commandQueue->Flush();
}

void Engine::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_lastMousePos.x = static_cast<float>(x);
    m_lastMousePos.y = static_cast<float>(y);

    SetCapture(Win32App::GetHwnd());
}

void Engine::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Engine::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

        m_camera->AdjustYaw(dx); 
        m_camera->AdjustPitch(dy);
    }

    m_lastMousePos.x = static_cast<float>(x);
    m_lastMousePos.y = static_cast<float>(y);
}

void Engine::OnKeyDown(UINT8 key)
{
}

void Engine::OnKeyUp(UINT8 key)
{
}

void Engine::OnKeyboardInput(const ScaldTimer& st)
{
    const float dt = st.DeltaTime();

#pragma region CameraMovement

    if (GetAsyncKeyState('W') & 0x8000)
        m_camera->MoveForward(10.0f * dt);
    
    if (GetAsyncKeyState('S') & 0x8000)
        m_camera->MoveForward(-10.0f * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        m_camera->MoveRight(-10.0f * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        m_camera->MoveRight(10.0f * dt);

    if (GetAsyncKeyState('Q') & 0x8000)
        m_camera->MoveUp(-8.f * dt);

    if (GetAsyncKeyState('E') & 0x8000)
        m_camera->MoveUp(8.f * dt);

#pragma endregion CameraMovement

    if (GetAsyncKeyState('1') & 0x8000)
        m_isWireframe = true;
    else
        m_isWireframe = false;

#pragma region GlobalLightDirection

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        m_sunTheta -= 1.0f * dt;

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        m_sunTheta += 1.0f * dt;

    if (GetAsyncKeyState(VK_UP) & 0x8000)
        m_sunPhi -= 1.0f * dt;
    
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        m_sunPhi += 1.0f * dt;

    m_sunPhi = ScaldMath::Clamp(m_sunPhi, 0.1f, XM_PIDIV2);

#pragma endregion GlobalLightDirection
}

void Engine::UpdateObjectsCB(const ScaldTimer& st)
{
    auto objectCB = m_currFrameResource->ObjectsCB.get();

    for (auto& ri : m_renderItems)
    {
        // Luna stuff. Try to remove 'if' statement.
        // Have tried. It does not affect anything. 
        // Looks like it just forces the code to update the object's constant buffer regardless of whether it has been modified or not.
        if (ri->NumFramesDirty > 0)
        {
            XMMATRIX transposeWorld = XMMatrixTranspose(ri->World);
            XMVECTOR det = XMMatrixDeterminant(transposeWorld);

            XMStoreFloat4x4(&m_perObjectCBData.World, transposeWorld);
            XMStoreFloat4x4(&m_perObjectCBData.InvTransposeWorld, XMMatrixTranspose(XMMatrixInverse(&det, transposeWorld)));
            XMStoreFloat4x4(&m_perObjectCBData.TexTransform, XMMatrixTranspose(ri->TexTransform));
            m_perObjectCBData.MaterialIndex = ri->Mat->MatBufferIndex;

            objectCB->CopyData(ri->ObjCBIndex, m_perObjectCBData); // In this case ri->ObjCBIndex would be equal to index 'i' of traditional for loop
            ri->NumFramesDirty--;
        }
    }
}

void Engine::UpdateMaterialBuffer(const ScaldTimer& st)
{
    auto currMaterialDataSB = m_currFrameResource->MaterialSB.get();

    for (auto& e : m_materials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            m_perMaterialSBData.DiffuseAlbedo = mat->DiffuseAlbedo;
            m_perMaterialSBData.FresnelR0 = mat->FresnelR0;
            m_perMaterialSBData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&m_perMaterialSBData.MatTransform, XMMatrixTranspose(mat->MatTransform));
            m_perMaterialSBData.DiffusseMapIndex = mat->DiffuseSrvHeapIndex;

            currMaterialDataSB->CopyData(mat->MatBufferIndex, m_perMaterialSBData);

            mat->NumFramesDirty--;
        }
    }
}

void Engine::UpdateLightsBuffer(const ScaldTimer& st)
{
    auto currPointLightSB = m_currFrameResource->PointLightSB.get();

    for (auto& e : m_pointLights)
    {
        // we have many instances, not the one objects, so think about it (we can't update all instances, if only one point light gets dirty)
        //if (e->NumFramesDirty > 0)
        //{
        
        int pointLightIndex = 0;
        const auto& instances = e->Instances;

        for (UINT i = 0; i < (UINT)instances.size(); ++i)
        {
            XMStoreFloat4x4(&m_perInstanceSBData.World, XMMatrixTranspose(XMLoadFloat4x4(&instances[i].World)));
            m_perInstanceSBData.Light.Strength = instances[i].Light.Strength;
            m_perInstanceSBData.Light.FallOfStart = instances[i].Light.FallOfStart;
            m_perInstanceSBData.Light.FallOfEnd = instances[i].Light.FallOfEnd;
            m_perInstanceSBData.Light.Position = instances[i].Light.Position;
            // copy all instances to structured buffer
            currPointLightSB->CopyData(pointLightIndex++, m_perInstanceSBData);
        }
        e->InstanceCount = pointLightIndex;

        //e->NumFramesDirty--;
        //}
    }
}

void Engine::UpdateShadowTransform(const ScaldTimer& st)
{
    std::vector<std::pair<XMMATRIX, XMMATRIX>> lightSpaceMatrices;
    GetLightSpaceMatrices(lightSpaceMatrices);

    for (UINT i = 0; i < MaxCascades; ++i)
    {
        XMMATRIX shadowTransform = lightSpaceMatrices[i].first * lightSpaceMatrices[i].second;
        m_shadowPassCBData.Cascades.CascadeViewProj[i] = XMMatrixTranspose(shadowTransform);

        m_mainPassCBData.Cascades.CascadeViewProj[i] = XMMatrixTranspose(shadowTransform);
        m_mainPassCBData.Cascades.Distances[i] = m_cascadeShadowMap->GetCascadeLevel(i);
    }
}

void Engine::UpdateShadowPassCB(const ScaldTimer& st)
{
    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixIdentity();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&m_shadowPassCBData.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_shadowPassCBData.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&m_shadowPassCBData.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&m_shadowPassCBData.InvViewProj, XMMatrixTranspose(invViewProj));

    auto currPassCB = m_currFrameResource->PassCB.get();
    currPassCB->CopyData(static_cast<int>(EPassType::DepthShadow), m_shadowPassCBData);
}

void Engine::UpdateGeometryPassCB(const ScaldTimer& st)
{
    XMMATRIX view = m_camera->GetViewMatrix();
    XMMATRIX proj = m_camera->GetPerspectiveProjectionMatrix();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&m_geometryPassCBData.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_geometryPassCBData.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&m_geometryPassCBData.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&m_geometryPassCBData.InvViewProj, XMMatrixTranspose(invViewProj));

    m_geometryPassCBData.EyePosW = m_camera->GetPosition3f();
    m_geometryPassCBData.RenderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
    m_geometryPassCBData.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
    m_geometryPassCBData.NearZ = m_camera->GetNearZ();
    m_geometryPassCBData.FarZ = m_camera->GetFarZ();
    m_geometryPassCBData.DeltaTime = st.DeltaTime();
    m_geometryPassCBData.TotalTime = st.TotalTime();

    auto currPassCB = m_currFrameResource->PassCB.get();
    currPassCB->CopyData(static_cast<int>(EPassType::DeferredGeometry), m_geometryPassCBData);
}

void Engine::UpdateMainPassCB(const ScaldTimer& st)
{
    XMMATRIX view = m_camera->GetViewMatrix();
    XMMATRIX proj = m_camera->GetPerspectiveProjectionMatrix();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&m_mainPassCBData.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_mainPassCBData.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&m_mainPassCBData.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&m_mainPassCBData.InvViewProj, XMMatrixTranspose(invViewProj));

    m_mainPassCBData.EyePosW = m_camera->GetPosition3f();
    m_mainPassCBData.RenderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
    m_mainPassCBData.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
    m_mainPassCBData.NearZ = m_camera->GetNearZ();
    m_mainPassCBData.FarZ = m_camera->GetFarZ();
    m_mainPassCBData.DeltaTime = st.DeltaTime();
    m_mainPassCBData.TotalTime = st.TotalTime();

    m_mainPassCBData.Ambient = { 0.25f, 0.25f, 0.35f, 1.0f };

#pragma region DirLight
    // Invert sign because other way light would be pointing up
    XMVECTOR lightDir = -ScaldMath::SphericalToCarthesian(1.0f, m_sunTheta, m_sunPhi);
    XMStoreFloat3(&m_mainPassCBData.DirLight.Direction, lightDir);
    m_mainPassCBData.DirLight.Strength = { 1.0f, 1.0f, 0.9f };
#pragma endregion DirLight

    auto currPassCB = m_currFrameResource->PassCB.get();
    currPassCB->CopyData(static_cast<int>(EPassType::DeferredLighting), m_mainPassCBData);
}

VOID Engine::PopulateCommandList(ID3D12GraphicsCommandList* pCommandList)
{
    // Set necessary state.
    pCommandList->SetGraphicsRootSignature(m_rootSignature->Get());

    // Access for setting and using root descriptor table
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    RenderDepthOnlyPass(pCommandList);
    RenderGeometryPass(pCommandList);
    RenderLightingPass(pCommandList);
    //RenderTransparencyPass(pCommandList);
}

void Engine::RenderDepthOnlyPass(ID3D12GraphicsCommandList* pCommandList)
{
    pCommandList->RSSetViewports(1u, &m_cascadeShadowMap->GetViewport());
    pCommandList->RSSetScissorRects(1u, &m_cascadeShadowMap->GetScissorRect());

    // change to depth write state
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_cascadeShadowMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pCommandList->ResourceBarrier(1u, &transition);

    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    pCommandList->SetGraphicsRootConstantBufferView(ERootParameter::PerPassDataCB, currFramePassCB->GetGPUVirtualAddress()); //cause shadow pass data lies in the first element

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_cascadeShadowMap->GetDsv());
    pCommandList->OMSetRenderTargets(0u, nullptr, TRUE, &dsvHandle);
    pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);

    pCommandList->SetPipelineState(m_pipelineStates.at(EPsoType::CascadedShadowsOpaque).Get());
    DrawRenderItems(pCommandList, m_renderItems);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_cascadeShadowMap->Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    pCommandList->ResourceBarrier(1u, &transition);
}

void Engine::RenderGeometryPass(ID3D12GraphicsCommandList* pCommandList)
{
    UINT passCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
    // The viewport needs to be reset whenever the command list is reset.
    pCommandList->RSSetViewports(1u, &m_viewport);
    pCommandList->RSSetScissorRects(1u, &m_scissorRect);

    // barrier
    for (unsigned i = 0; i < GBuffer::EGBufferLayer::MAX - 1u; i++)
    {
        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->Get(i), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        pCommandList->ResourceBarrier(1u, &transition);
    }
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->Get(GBuffer::EGBufferLayer::DEPTH), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pCommandList->ResourceBarrier(1u, &transition);

#pragma region BypassResources
    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    pCommandList->SetGraphicsRootConstantBufferView(ERootParameter::PerPassDataCB, currFramePassCB->GetGPUVirtualAddress() + passCBByteSize); // second element contains data for geometry pass

    // Bind all the materials used in this scene. For structured buffers, we can bypass the heap and set as a root descriptor.
    auto matBuffer = m_currFrameResource->MaterialSB->Get();
    pCommandList->SetGraphicsRootShaderResourceView(ERootParameter::MaterialDataSB, matBuffer->GetGPUVirtualAddress());

    // Bind all the textures used in this scene. Observe that we only have to specify the first descriptor in the table.  
    // The root signature knows how many descriptors are expected in the table.
    pCommandList->SetGraphicsRootDescriptorTable(ERootParameter::Textures, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
#pragma endregion BypassResources
    
    // start of the GBuffer rtvs in rtvHeap
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainFrameCount, m_rtvDescriptorSize);

    // Before. I left it here just for clarification, the approach above more preferable
    /*D3D12_CPU_DESCRIPTOR_HANDLE* rtvs[] = {
        &m_GBuffer->GetRtv(GBuffer::EGBufferLayer::DIFFUSE_ALBEDO),
        &m_GBuffer->GetRtv(GBuffer::EGBufferLayer::AMBIENT_OCCLUSION),
        &m_GBuffer->GetRtv(GBuffer::EGBufferLayer::NORMAL),
        &m_GBuffer->GetRtv(GBuffer::EGBufferLayer::SPECULAR),
    };*/

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_GBuffer->GetDsv(GBuffer::EGBufferLayer::DEPTH));
    pCommandList->OMSetRenderTargets(GBuffer::EGBufferLayer::DEPTH, &rtvHandle, TRUE, &dsvHandle);

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCommandList->ClearRenderTargetView(m_GBuffer->GetRtv(GBuffer::EGBufferLayer::DIFFUSE_ALBEDO), Colors::LightSteelBlue, 0u, nullptr);
    pCommandList->ClearRenderTargetView(m_GBuffer->GetRtv(GBuffer::EGBufferLayer::AMBIENT_OCCLUSION), clearColor, 0u, nullptr);
    pCommandList->ClearRenderTargetView(m_GBuffer->GetRtv(GBuffer::EGBufferLayer::NORMAL), clearColor, 0u, nullptr);
    pCommandList->ClearRenderTargetView(m_GBuffer->GetRtv(GBuffer::EGBufferLayer::SPECULAR), clearColor, 0u, nullptr);
    pCommandList->ClearDepthStencilView(m_GBuffer->GetDsv(GBuffer::EGBufferLayer::DEPTH), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);

    pCommandList->SetPipelineState(m_pipelineStates.at(EPsoType::DeferredGeometry).Get());
    DrawRenderItems(pCommandList, m_renderItems);

    for (unsigned i = 0; i < GBuffer::EGBufferLayer::MAX - 1u; i++)
    {
        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->Get(i), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        pCommandList->ResourceBarrier(1u, &transition);
    }

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->Get(GBuffer::EGBufferLayer::DEPTH), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    pCommandList->ResourceBarrier(1u, &transition);
}

void Engine::RenderLightingPass(ID3D12GraphicsCommandList* pCommandList)
{
    DeferredDirectionalLightPass(pCommandList);
    DeferredPointLightPass(pCommandList);
    //DeferredSpotLightPass(pCommandList);
}

void Engine::DeferredDirectionalLightPass(ID3D12GraphicsCommandList* pCommandList)
{
    UINT passCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Indicate that the back buffer will be used as a render target.
    pCommandList->ResourceBarrier(1u, &transition);

    // The viewport needs to be reset whenever the command list is reset.
    pCommandList->RSSetViewports(1u, &m_viewport);
    pCommandList->RSSetScissorRects(1u, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currBackBuffer, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    const float* clearColor = &m_mainPassCBData.FogColor.x;
    pCommandList->OMSetRenderTargets(1u, &rtvHandle, TRUE, &dsvHandle);
    pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);

#pragma region BypassResources
    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    pCommandList->SetGraphicsRootConstantBufferView(ERootParameter::PerPassDataCB, currFramePassCB->GetGPUVirtualAddress() + 2u * passCBByteSize); // third element contains data for color/light pass

    // Set shaadow map texture for main pass
    pCommandList->SetGraphicsRootDescriptorTable(ERootParameter::CascadedShadowMaps, m_cascadeShadowSrv);

    // Bind GBuffer textures
    pCommandList->SetGraphicsRootDescriptorTable(ERootParameter::GBufferTextures, m_GBufferTexturesSrv);
#pragma endregion BypassResources

    pCommandList->SetPipelineState(m_pipelineStates.at(EPsoType::DeferredDirectional).Get());
    DrawQuad(pCommandList);
}

void Engine::DeferredPointLightPass(ID3D12GraphicsCommandList* pCommandList)
{
    UINT passCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    pCommandList->SetGraphicsRootConstantBufferView(ERootParameter::PerPassDataCB, currFramePassCB->GetGPUVirtualAddress() + 2u * passCBByteSize); // third element contains data for color/light pass

    // Bind GBuffer textures
    pCommandList->SetGraphicsRootDescriptorTable(ERootParameter::GBufferTextures, m_GBufferTexturesSrv);

    // !!! HACK (TO DRAW EVEN IF FRUSTUM INTERSECTS LIGHT VOLUME)
    pCommandList->SetPipelineState(m_pipelineStates.at(EPsoType::DeferredPointWithinFrustum).Get());
    DrawInstancedRenderItems(pCommandList, m_pointLights);

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currBackBuffer].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // Indicate that the back buffer will now be used to present.
    pCommandList->ResourceBarrier(1u, &transition);
}

void Engine::DeferredSpotLightPass(ID3D12GraphicsCommandList* pCommandList)
{

}

void Engine::RenderTransparencyPass(ID3D12GraphicsCommandList* pCommandList)
{
    // forward-like
}

void Engine::DrawRenderItems(ID3D12GraphicsCommandList* pCommandList, std::vector<std::unique_ptr<RenderItem>>& renderItems)
{
    UINT objCBByteSize = (UINT)ScaldUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto currFrameObjCB = m_currFrameResource->ObjectsCB->Get();       // Get actual ID3D12Resource*

    for (auto& ri : renderItems)
    {
        pCommandList->IASetPrimitiveTopology(ri->PrimitiveTopologyType);
        pCommandList->IASetVertexBuffers(0u, 1u, &ri->Geo->VertexBufferView());
        pCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = currFrameObjCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        // now we set only objects' cbv per item, material data is set per pass
        // we get material data by index from structured buffer
        pCommandList->SetGraphicsRootConstantBufferView(ERootParameter::PerObjectDataCB, objCBAddress);

        pCommandList->DrawIndexedInstanced(ri->IndexCount, 1u, ri->StartIndexLocation, ri->BaseVertexLocation, 0u);
    }
}

void Engine::DrawInstancedRenderItems(ID3D12GraphicsCommandList* pCommandList, std::vector<std::unique_ptr<RenderItem>>& renderItems)
{
    for (auto& ri : renderItems)
    {
        pCommandList->IASetPrimitiveTopology(ri->PrimitiveTopologyType);
        pCommandList->IASetVertexBuffers(0u, 1u, &ri->Geo->VertexBufferView());
        pCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());

        // Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
        // the heap and set as a root descriptor.
        auto instanceBuffer = m_currFrameResource->PointLightSB->Get();

        // now we set only objects' cbv per item, material data is set per pass
        // we get material data by index from structured buffer
        pCommandList->SetGraphicsRootShaderResourceView(ERootParameter::PointLightsDataSB, instanceBuffer->GetGPUVirtualAddress());

        pCommandList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0u);
    }
}

void Engine::DrawQuad(ID3D12GraphicsCommandList* pCommandList)
{
    auto currFrameObjCB = m_currFrameResource->ObjectsCB->Get();

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pCommandList->IASetVertexBuffers(0u, 1u, &m_geometries.at("fullQuad")->VertexBufferView());

    pCommandList->DrawInstanced(4u, 1u, 0u, 0u);
}

std::pair<XMMATRIX, XMMATRIX> Engine::GetLightSpaceMatrix(const float nearZ, const float farZ)
{
    const auto directionalLight = m_mainPassCBData.DirLight;

    const XMFLOAT3 lightDir = directionalLight.Direction;

    const auto cameraProj = XMMatrixPerspectiveFovLH(m_camera->GetFovYRad(), m_aspectRatio, nearZ, farZ);
    const auto frustumCorners = GetFrustumCornersWorldSpace(m_camera->GetViewMatrix(), cameraProj);

    XMVECTOR center = XMVectorZero();
    for (const auto& v : frustumCorners)
    {
        center += v;
    }
    center /= (float)frustumCorners.size();

    const XMMATRIX lightView = XMMatrixLookAtLH(center, center + XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 1.0f), ScaldMath::UpVector);

    // Measuring cascade
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (const auto& v : frustumCorners)
    {
        const auto trf = XMVector4Transform(v, lightView);

        minX = std::min(minX, XMVectorGetX(trf));
        maxX = std::max(maxX, XMVectorGetX(trf));
        minY = std::min(minY, XMVectorGetY(trf));
        maxY = std::max(maxY, XMVectorGetY(trf));
        minZ = std::min(minZ, XMVectorGetZ(trf));
        maxZ = std::max(maxZ, XMVectorGetZ(trf));
    }
    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
    minZ = (minZ < 0) ? minZ * zMult : minZ / zMult;
    maxZ = (maxZ < 0) ? maxZ / zMult : maxZ * zMult;

    const XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

    return std::make_pair(lightView, lightProj);
}

void Engine::GetLightSpaceMatrices(std::vector<std::pair<XMMATRIX, XMMATRIX>>& outMatrices)
{
    for (UINT i = 0u; i < MaxCascades; ++i)
    {
        if (i == 0u)
        {
            outMatrices.push_back(GetLightSpaceMatrix(m_camera->GetNearZ(), m_cascadeShadowMap->GetCascadeLevel(i)));
        }
        else
        {
            outMatrices.push_back(GetLightSpaceMatrix(m_cascadeShadowMap->GetCascadeLevel(i-1), m_cascadeShadowMap->GetCascadeLevel(i)));
        }
    }
}

std::vector<XMVECTOR> Engine::GetFrustumCornersWorldSpace(const XMMATRIX& view, const XMMATRIX& proj)
{
    const auto viewProj = view * proj;

    XMVECTOR det = XMMatrixDeterminant(viewProj);
    const auto invViewProj = XMMatrixInverse(&det, viewProj);

    std::vector<XMVECTOR> frustumCorners;
    frustumCorners.reserve(8);

    for (UINT x = 0; x < 2; ++x)
    {
        for (UINT y = 0; y < 2; ++y)
        {
            for (UINT z = 0; z < 2; ++z)
            {
                // translate NDC coords to world space
                const XMVECTOR pt = XMVector4Transform(
                    XMVectorSet(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        (float)z,
                        1.0f), invViewProj);
                frustumCorners.push_back(pt / XMVectorGetW(pt));
            }
        }
    }
    return frustumCorners;
}