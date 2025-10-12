#include "stdafx.h"
#include "Engine.h"
#include "Shapes.h"
#include "Common/ScaldMath.h"
#include "GameFramework/Components/Transform.h"
#include "GameFramework/Components/Renderer.h"

extern const int gNumFrameResources;

Engine::Engine(UINT width, UINT height, std::wstring name, std::wstring className) 
    : 
    D3D12Sample(width, height, name, className)
{
    m_camera = std::make_unique<Camera>();
}

Engine::~Engine()
{
    if (m_device)
    {
        //FlushCommandQueue();
    }
}

void Engine::OnInit()
{
    LoadPipeline();

    m_cascadeShadowMap = std::make_unique<CascadeShadowMap>(m_device.Get(), 2048u, 2048u, MaxCascades);
    CreateShadowCascadeSplits();

    LoadAssets();
}

// Load the rendering pipeline dependencies.
VOID Engine::LoadPipeline()
{
    D3D12Sample::LoadPipeline();
}

// Load the sample assets.
VOID Engine::LoadAssets()
{
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
    
    LoadTextures();
    CreateGeometry();
    CreateGeometryMaterials();
    CreateRenderItems();
    CreateFrameResources();
    CreateDescriptorHeaps();
    CreateRootSignature();
    // Create the pipeline state, which includes compiling and loading shaders.
    {
        CreateShaders();
        CreatePSO();
    }

    m_commandList->Close();
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

VOID Engine::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, /* number of textures 2D */(UINT)m_textures.size(), 1u, 0u);

    CD3DX12_DESCRIPTOR_RANGE cascadeShadowSrv;
    cascadeShadowSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];
    
    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0u, 0u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialIndex used in both shaders */);  // a root descriptor for objects' CBVs.
    slotRootParameter[1].InitAsConstantBufferView(1u, 0u, D3D12_SHADER_VISIBILITY_ALL);                                            // a root descriptor for Pass CBV.
    slotRootParameter[2].InitAsShaderResourceView(0u, 1u, D3D12_SHADER_VISIBILITY_ALL /* gMaterialData used in both shaders */);   // a srv for structured buffer with materials' data
    slotRootParameter[3].InitAsDescriptorTable(1u, &cascadeShadowSrv, D3D12_SHADER_VISIBILITY_PIXEL);                              // a descriptor table for shadow maps array.
    slotRootParameter[4].InitAsDescriptorTable(1u, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);                                      // a descriptor table for textures

    auto staticSamplers = GetStaticSamplers();

    // Root signature is an array of root parameters
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(ARRAYSIZE(slotRootParameter), slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature = nullptr;
    ComPtr<ID3DBlob> error = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0u, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
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

    m_shaders["defaultVS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/VertexShader.hlsl", nullptr, "main", "vs_5_1");
    m_shaders["opaquePS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/PixelShader.hlsl", fogDefines, "main", "ps_5_1");
    m_shaders["shadowGS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/GeometryShader.hlsl", nullptr, "main", "gs_5_1");
    m_shaders["shadowVS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/ShadowVertexShader.hlsl", nullptr, "main", "vs_5_1");

#pragma region DeferredShading
    m_shaders["gBufferVS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/GBufferPassVS.hlsl", nullptr, "main", "vs_5_1");
    m_shaders["gBufferPS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/GBufferPassPS.hlsl", nullptr, "main", "ps_5_1");
    m_shaders["deferredLightVS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredLightVS.hlsl", nullptr, "main", "vs_5_1");
    m_shaders["deferredLightPS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/DeferredLightPS.hlsl", nullptr, "main", "ps_5_1");
#pragma endregion DeferredShading

    m_inputLayout =
    {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
        { "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 12u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
        { "TEXCOORD", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 24u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
    };
}

VOID Engine::CreatePSO()
{
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    opaquePsoDesc.pRootSignature = m_rootSignature.Get();
    opaquePsoDesc.VS = D3D12_SHADER_BYTECODE(
        { 
            reinterpret_cast<BYTE*>(m_shaders.at("defaultVS")->GetBufferPointer()), 
            m_shaders.at("defaultVS")->GetBufferSize()
        });
    opaquePsoDesc.PS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at("opaquePS")->GetBufferPointer()),
            m_shaders.at("opaquePS")->GetBufferSize()
        });

    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // Blend state is disable
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size()};
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1u;
    opaquePsoDesc.RTVFormats[0] = BackBufferFormat;
    opaquePsoDesc.DSVFormat = DepthStencilFormat;
    // Do not use multisampling
    // This should match the setting of the render target we are using (check swapChainDesc)
    opaquePsoDesc.SampleDesc.Count = 1u;
    opaquePsoDesc.SampleDesc.Quality = 0u;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_pipelineStates["opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframe = opaquePsoDesc;
    opaqueWireframe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaqueWireframe, IID_PPV_ARGS(&m_pipelineStates["opaque_wireframe"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
    
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
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&m_pipelineStates["transparency"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = {};
    ZeroMemory(&shadowPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    shadowPsoDesc.pRootSignature = m_rootSignature.Get();
    shadowPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at("defaultVS")->GetBufferPointer()),
            m_shaders.at("defaultVS")->GetBufferSize()
        });
    shadowPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // Blend state is disable
    shadowPsoDesc.SampleMask = UINT_MAX;
    shadowPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    shadowPsoDesc.RasterizerState.DepthBias = 10000;
    shadowPsoDesc.RasterizerState.DepthClipEnable = (BOOL)0.0f;
    shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    shadowPsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowPsoDesc.NumRenderTargets = 0u;
    shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    shadowPsoDesc.DSVFormat = DepthStencilFormat;
    shadowPsoDesc.SampleDesc.Count = 1u;
    shadowPsoDesc.SampleDesc.Quality = 0u;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&m_pipelineStates["shadow_opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC cascadeShadowPsoDesc = shadowPsoDesc;
    cascadeShadowPsoDesc.VS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at("shadowVS")->GetBufferPointer()),
                m_shaders.at("shadowVS")->GetBufferSize()
        });
    cascadeShadowPsoDesc.GS = D3D12_SHADER_BYTECODE(
        {
            reinterpret_cast<BYTE*>(m_shaders.at("shadowGS")->GetBufferPointer()),
                m_shaders.at("shadowGS")->GetBufferSize()
        });

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&cascadeShadowPsoDesc, IID_PPV_ARGS(&m_pipelineStates["cascades_opaque"])));
}

VOID Engine::LoadTextures()
{
    auto brickTex = std::make_unique<Texture>();
    brickTex->Name = "brickTex";
    brickTex->Filename = L"./Assets/Textures/bricks.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        brickTex->Filename.c_str(), brickTex->Resource, brickTex->UploadHeap));

    auto grassTex = std::make_unique<Texture>();
    grassTex->Name = "grassTex";
    grassTex->Filename = L"./Assets/Textures/grass.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        grassTex->Filename.c_str(), grassTex->Resource, grassTex->UploadHeap));

    auto iceTex = std::make_unique<Texture>();
    iceTex->Name = "iceTex";
    iceTex->Filename = L"./Assets/Textures/ice.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        iceTex->Filename.c_str(), iceTex->Resource, iceTex->UploadHeap));

    auto stoneTex = std::make_unique<Texture>();
    stoneTex->Name = "stoneTex";
    stoneTex->Filename = L"./Assets/Textures/stone.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        stoneTex->Filename.c_str(), stoneTex->Resource, stoneTex->UploadHeap));

    auto tileTex = std::make_unique<Texture>();
    tileTex->Name = "tileTex";
    tileTex->Filename = L"./Assets/Textures/tile.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        tileTex->Filename.c_str(), tileTex->Resource, tileTex->UploadHeap));

    auto planksTex = std::make_unique<Texture>();
    planksTex->Name = "planksTex";
    planksTex->Filename = L"./Assets/Textures/planks.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(),
        planksTex->Filename.c_str(), planksTex->Resource, planksTex->UploadHeap));

    m_textures[brickTex->Name] = std::move(brickTex);
    m_textures[grassTex->Name] = std::move(grassTex);
    m_textures[iceTex->Name] = std::move(iceTex);
    m_textures[stoneTex->Name] = std::move(stoneTex);
    m_textures[tileTex->Name] = std::move(tileTex);
    m_textures[planksTex->Name] = std::move(planksTex);
}

VOID Engine::CreateGeometry()
{
    // Just meshes
    MeshData box = Shapes::CreateBox(1.0f, 1.0f, 1.0f);
    // the same mesh for all planets
    MeshData sphere = Shapes::CreateSphere(1.0f, 16u, 16u);

    MeshData grid = Shapes::CreateGrid(100.0f, 100.0f, 2u, 2u);

    // Create shared vertex/index buffer for all geometry.
    UINT sunVertexOffset = 0u;
    UINT mercuryVertexOffset = sunVertexOffset + (UINT)sphere.vertices.size();
    UINT venusVertexOffset = mercuryVertexOffset + (UINT)sphere.vertices.size();
    UINT earthVertexOffset = venusVertexOffset + (UINT)sphere.vertices.size();
    UINT marsVertexOffset = earthVertexOffset + (UINT)sphere.vertices.size();
    UINT planeVertexOffset = marsVertexOffset + (UINT)sphere.vertices.size();

    UINT sunIndexOffset = 0u;
    UINT mercuryIndexOffset = sunIndexOffset + (UINT)sphere.indices.size();
    UINT venusIndexOffset = mercuryIndexOffset + (UINT)sphere.indices.size();
    UINT earthIndexOffset = venusIndexOffset + (UINT)sphere.indices.size();
    UINT marsIndexOffset = earthIndexOffset + (UINT)sphere.indices.size();
    UINT planeIndexOffset = marsIndexOffset + (UINT)sphere.indices.size();

    SubmeshGeometry sunSubmesh;
    sunSubmesh.IndexCount = (UINT)sphere.indices.size();
    sunSubmesh.StartIndexLocation = sunIndexOffset;
    sunSubmesh.BaseVertexLocation = sunVertexOffset;
    sunSubmesh.Bounds = sphere.Bounds;

    SubmeshGeometry mercurySubmesh;
    mercurySubmesh.IndexCount = (UINT)sphere.indices.size();
    mercurySubmesh.StartIndexLocation = mercuryIndexOffset;
    mercurySubmesh.BaseVertexLocation = mercuryVertexOffset;
    mercurySubmesh.Bounds = sphere.Bounds;

    SubmeshGeometry venusSubmesh;
    venusSubmesh.IndexCount = (UINT)sphere.indices.size();
    venusSubmesh.StartIndexLocation = venusIndexOffset;
    venusSubmesh.BaseVertexLocation = venusVertexOffset;
    venusSubmesh.Bounds = sphere.Bounds;

    SubmeshGeometry earthSubmesh;
    earthSubmesh.IndexCount = (UINT)sphere.indices.size();
    earthSubmesh.StartIndexLocation = earthIndexOffset;
    earthSubmesh.BaseVertexLocation = earthVertexOffset;
    earthSubmesh.Bounds = sphere.Bounds;

    SubmeshGeometry marsSubmesh;
    marsSubmesh.IndexCount = (UINT)sphere.indices.size();
    marsSubmesh.StartIndexLocation = marsIndexOffset;
    marsSubmesh.BaseVertexLocation = marsVertexOffset;
    marsSubmesh.Bounds = sphere.Bounds;;

    SubmeshGeometry planeSubmesh;
    planeSubmesh.IndexCount = (UINT)grid.indices.size();
    planeSubmesh.StartIndexLocation = planeIndexOffset;
    planeSubmesh.BaseVertexLocation = planeVertexOffset;
    planeSubmesh.BaseVertexLocation = planeVertexOffset;

    auto totalVertexCount = 
        sphere.vertices.size() // sun
        + sphere.vertices.size() // merc
        + sphere.vertices.size() // venus
        + sphere.vertices.size() //earth
        + sphere.vertices.size() //mars
        + grid.vertices.size() //mars
        ;
    auto totalIndexCount = 
        sphere.indices.size()
        + sphere.indices.size()
        + sphere.indices.size()
        + sphere.indices.size()
        + sphere.indices.size()
        + grid.indices.size()
        ;

    std::vector<Vertex> vertices(totalVertexCount);

    int k = 0;
    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
        vertices[k].texC = sphere.vertices[i].texCoord;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
        vertices[k].texC = sphere.vertices[i].texCoord;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
        vertices[k].texC = sphere.vertices[i].texCoord;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
        vertices[k].texC = sphere.vertices[i].texCoord;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
        vertices[k].texC = sphere.vertices[i].texCoord;
    }

    for (size_t i = 0; i < grid.vertices.size(); ++i, ++k)
    {
        vertices[k].position = grid.vertices[i].position;
        vertices[k].normal = grid.vertices[i].normal;
        vertices[k].texC = grid.vertices[i].texCoord;
    }

    std::vector<uint16_t> indices;
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), grid.indices.begin(), grid.indices.end());

    const UINT64 vbByteSize = vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geometry = std::make_unique<MeshGeometry>();
    geometry->Name = "solarSystem";
    
    // Create system buffer for copy vertices data
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
    CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    // Create GPU resource
    geometry->VertexBufferGPU = ScaldUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), vertices.data(), vbByteSize, geometry->VertexBufferUploader);
    // Initialize the vertex buffer view.
    geometry->VertexBufferByteSize = (UINT)vbByteSize;
    geometry->VertexByteStride = sizeof(Vertex);

    // Create system buffer for copy indices data
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
    CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
    // Create GPU resource
    geometry->IndexBufferGPU = ScaldUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indices.data(), ibByteSize, geometry->IndexBufferUploader);
    // Initialize the index buffer view.
    geometry->IndexBufferByteSize = ibByteSize;
    geometry->IndexFormat = DXGI_FORMAT_R16_UINT;

    geometry->DrawArgs["sun"] = sunSubmesh;
    geometry->DrawArgs["mercury"] = mercurySubmesh;
    geometry->DrawArgs["venus"] = venusSubmesh;
    geometry->DrawArgs["earth"] = earthSubmesh;
    geometry->DrawArgs["mars"] = marsSubmesh;
    geometry->DrawArgs["plane"] = planeSubmesh;

    m_geometries[geometry->Name] = std::move(geometry);
}

VOID Engine::CreateGeometryMaterials()
{
    // DiffuseAlbedo in materials is set (1,1,1,1) by default to not affect texture diffuse albedo
    auto flame0 = std::make_unique<Material>();
    flame0->Name = "flame0";
    flame0->MatBufferIndex = 0;
    flame0->DiffuseSrvHeapIndex = 0;
    //flame0->DiffuseAlbedo = XMFLOAT4(Colors::Gold);
    flame0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    flame0->Roughness = 0.2f;
    flame0->MatTransform = XMMatrixIdentity();

    auto sand0 = std::make_unique<Material>();
    sand0->Name = "sand0";
    sand0->MatBufferIndex = 1;
    sand0->DiffuseSrvHeapIndex = 1;
    //sand0->DiffuseAlbedo = XMFLOAT4(Colors::Brown);
    sand0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    sand0->Roughness = 0.1f;
    sand0->MatTransform = XMMatrixIdentity();

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatBufferIndex = 2;
    stone0->DiffuseSrvHeapIndex = 2;
    //stone0->DiffuseAlbedo = XMFLOAT4(Colors::Orchid);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;
    stone0->MatTransform = XMMatrixIdentity();

    auto ground0 = std::make_unique<Material>();
    ground0->Name = "ground0";
    ground0->MatBufferIndex = 3;
    ground0->DiffuseSrvHeapIndex = 3;
    //ground0->DiffuseAlbedo = XMFLOAT4(Colors::Green);
    ground0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    ground0->Roughness = 0.3f;
    ground0->MatTransform = XMMatrixIdentity();

    auto wood0 = std::make_unique<Material>();
    wood0->Name = "wood0";
    wood0->MatBufferIndex = 4;
    wood0->DiffuseSrvHeapIndex = 4;
    wood0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    wood0->Roughness = 0.5f;
    wood0->MatTransform = XMMatrixIdentity();
    
    auto iron0 = std::make_unique<Material>();
    iron0->Name = "iron0";
    iron0->MatBufferIndex = 5;
    iron0->DiffuseSrvHeapIndex = 5;
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
    auto sunRenderItem = std::make_unique<RenderItem>();
    sunRenderItem->World = XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    sunRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    sunRenderItem->Geo = m_geometries.at("solarSystem").get();
    sunRenderItem->Mat = m_materials.at("flame0").get();
    sunRenderItem->ObjCBIndex = 0u;
    sunRenderItem->IndexCount = sunRenderItem->Geo->DrawArgs.at("sun").IndexCount;
    sunRenderItem->StartIndexLocation = sunRenderItem->Geo->DrawArgs.at("sun").StartIndexLocation;
    sunRenderItem->BaseVertexLocation = sunRenderItem->Geo->DrawArgs.at("sun").BaseVertexLocation;
    sunRenderItem->Bounds = sunRenderItem->Geo->DrawArgs.at("sun").Bounds;

    auto mercuryRenderItem = std::make_unique<RenderItem>();
    mercuryRenderItem->World = XMMatrixScaling(0.25f, 0.25f, 0.25f) * XMMatrixTranslation(2.0f, 0.0f, 0.0f);
    mercuryRenderItem->Geo = m_geometries.at("solarSystem").get();
    mercuryRenderItem->Mat = m_materials.at("sand0").get();
    mercuryRenderItem->ObjCBIndex = 1u;
    mercuryRenderItem->IndexCount = mercuryRenderItem->Geo->DrawArgs.at("mercury").IndexCount;
    mercuryRenderItem->StartIndexLocation = mercuryRenderItem->Geo->DrawArgs.at("mercury").StartIndexLocation;
    mercuryRenderItem->BaseVertexLocation = mercuryRenderItem->Geo->DrawArgs.at("mercury").BaseVertexLocation;
    mercuryRenderItem->Bounds = mercuryRenderItem->Geo->DrawArgs.at("mercury").Bounds;

    auto venusRenderItem = std::make_unique<RenderItem>();
    venusRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(3.0f, 0.0f, 3.0f);
    venusRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    venusRenderItem->Geo = m_geometries.at("solarSystem").get();
    venusRenderItem->Mat = m_materials.at("stone0").get();
    venusRenderItem->ObjCBIndex = 2u;
    venusRenderItem->IndexCount = venusRenderItem->Geo->DrawArgs.at("venus").IndexCount;
    venusRenderItem->StartIndexLocation = venusRenderItem->Geo->DrawArgs.at("venus").StartIndexLocation;
    venusRenderItem->BaseVertexLocation = venusRenderItem->Geo->DrawArgs.at("venus").BaseVertexLocation;
    venusRenderItem->Bounds = venusRenderItem->Geo->DrawArgs.at("venus").Bounds;

    auto earthRenderItem = std::make_unique<RenderItem>();
    earthRenderItem->World = XMMatrixScaling(0.6f, 0.6f, 0.6f) * XMMatrixTranslation(4.0f, 0.0f, 4.0f);
    earthRenderItem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
    earthRenderItem->Geo = m_geometries.at("solarSystem").get();
    earthRenderItem->Mat = m_materials.at("ground0").get();
    earthRenderItem->ObjCBIndex = 3u;
    earthRenderItem->IndexCount = earthRenderItem->Geo->DrawArgs.at("earth").IndexCount;
    earthRenderItem->StartIndexLocation = earthRenderItem->Geo->DrawArgs.at("earth").StartIndexLocation;
    earthRenderItem->BaseVertexLocation = earthRenderItem->Geo->DrawArgs.at("earth").BaseVertexLocation;
    earthRenderItem->Bounds = earthRenderItem->Geo->DrawArgs.at("earth").Bounds;

    auto marsRenderItem = std::make_unique<RenderItem>();
    marsRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(5.0f, 0.0f, 5.0f);
    marsRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    marsRenderItem->Geo = m_geometries.at("solarSystem").get();
    marsRenderItem->Mat = m_materials.at("iron0").get();
    marsRenderItem->ObjCBIndex = 4u;
    marsRenderItem->IndexCount = marsRenderItem->Geo->DrawArgs.at("mars").IndexCount;
    marsRenderItem->StartIndexLocation = marsRenderItem->Geo->DrawArgs.at("mars").StartIndexLocation;
    marsRenderItem->BaseVertexLocation = marsRenderItem->Geo->DrawArgs.at("mars").BaseVertexLocation;
    marsRenderItem->Bounds = marsRenderItem->Geo->DrawArgs.at("mars").Bounds;

    auto planeRenderItem = std::make_unique<RenderItem>();
    planeRenderItem->World = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, -1.5f, 0.0f);
    planeRenderItem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
    planeRenderItem->Geo = m_geometries.at("solarSystem").get();
    planeRenderItem->Mat = m_materials.at("wood0").get();
    planeRenderItem->ObjCBIndex = 5u;
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

VOID Engine::CreateFrameResources()
{
    for (int i = 0; i < gNumFrameResources; i++)
    {
                                                                                 // shadow pass + main      
        m_frameResources.push_back(std::make_unique<FrameResource>(m_device.Get(), 2u, (UINT)m_renderItems.size(), (UINT)m_materials.size()));
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
                                            // textures + csm
    srvHeapDesc.NumDescriptors = (UINT)m_textures.size() + 1u;
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

    auto srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto dsvCpuStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    m_cascadeShadowSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize);
    m_cascadeShadowMap->CreateDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize),
        CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, m_cascadeShadowMapHeapIndex, m_cbvSrvUavDescriptorSize),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, m_dsvDescriptorSize));
}

VOID Engine::Reset()
{
    D3D12Sample::Reset();

    // Init/Reinit camera
    m_camera->Reset(75.0f, m_aspectRatio, 1.0f, 250.0f);
}

VOID Engine::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = SwapChainFrameCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 1 dsv + 1 cascade shadow map
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 2u;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// Update frame-based values.
void Engine::OnUpdate(const ScaldTimer& st)
{
    OnKeyboardInput(st);
    m_camera->Update(st.DeltaTime());
    
    // Cycle through the circular frame resource array.
    m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % gNumFrameResources;
    m_currFrameResource = m_frameResources[m_currFrameResourceIndex].get();

    UpdateObjectsCB(st);
    UpdateMaterialBuffer(st);
    
    UpdateShadowTransform(st);
    UpdateShadowPassCB(st); // pass

    UpdateMainPassCB(st); // pass
}

// Render the scene.
void Engine::OnRender(const ScaldTimer& st)
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1u, 0u));

    MoveToNextFrame();
}

void Engine::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGPU();

    CloseHandle(m_fenceEvent);
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
            XMStoreFloat4x4(&m_perObjectCBData.World, XMMatrixTranspose(ri->World));
            XMStoreFloat4x4(&m_perObjectCBData.TexTransform, XMMatrixTranspose(ri->TexTransform));
            m_perObjectCBData.MaterialIndex = ri->Mat->MatBufferIndex;

            objectCB->CopyData(ri->ObjCBIndex, m_perObjectCBData); // In this case ri->ObjCBIndex would be equal to index 'i' of traditional for loop
            ri->NumFramesDirty--;
        }
    }
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
    m_mainPassCBData.NearZ = m_camera->GetNearZ();
    m_mainPassCBData.FarZ = m_camera->GetFarZ();
    m_mainPassCBData.DeltaTime = st.DeltaTime();
    m_mainPassCBData.TotalTime = st.TotalTime();

    // Invert sign because other way light would be pointing up
    XMVECTOR lightDir = -ScaldMath::SphericalToCarthesian(1.0f, m_sunTheta, m_sunPhi);

    m_mainPassCBData.Ambient = { 0.25f, 0.25f, 0.35f, 1.0f };

#pragma region DirLights
    XMStoreFloat3(&m_mainPassCBData.Lights[0].Direction, lightDir);
    m_mainPassCBData.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };
#pragma endregion DirLights

#pragma region PointLights
    m_mainPassCBData.Lights[1].Position = {0.0f, 0.0f, 2.0f};
    m_mainPassCBData.Lights[1].FallOfStart = 5.0f;
    m_mainPassCBData.Lights[1].FallOfEnd = 10.0f;
    m_mainPassCBData.Lights[1].Strength = { 0.9f, 0.5f, 0.9f };
#pragma endregion PointLights

    auto currPassCB = m_currFrameResource->PassCB.get();
    currPassCB->CopyData(/*main pass data in 0 element*/0, m_mainPassCBData);
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

void Engine::UpdateShadowTransform(const ScaldTimer& st)
{
    std::vector<std::pair<XMMATRIX, XMMATRIX>> lightSpaceMatrices;
    GetLightSpaceMatrices(lightSpaceMatrices);

    for (UINT i = 0; i < MaxCascades; ++i)
    {
        XMMATRIX shadowTransform = lightSpaceMatrices[i].first * lightSpaceMatrices[i].second;
        m_shadowPassCBData.Cascades.CascadeViewProj[i] = XMMatrixTranspose(shadowTransform);

        m_mainPassCBData.Cascades.CascadeViewProj[i] = XMMatrixTranspose(shadowTransform);
        m_mainPassCBData.Cascades.Distances[i] = m_shadowCascadeLevels[i];
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
    currPassCB->CopyData(/*shadow pass data in 1 element*/1, m_shadowPassCBData);
}

VOID Engine::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated command lists have finished execution on the GPU;
    // apps should use fences to determine GPU execution progress.
    auto currentCmdAlloc = m_currFrameResource->commandAllocator.Get();

    ThrowIfFailed(currentCmdAlloc->Reset());

    // However, when ExecuteCommandList() is called on a particular command list,
    // that command list can then be reset at any time and must be before re-recording.
    if (m_isWireframe)
    {
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates.at("opaque_wireframe").Get()));
    }
    else
    {
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates.at("opaque").Get()));
    }

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Access for setting and using root descriptor table
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    RenderDepthOnlyPass();

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1u, &transition);

    // The viewport needs to be reset whenever the command list is reset.
    m_commandList->RSSetViewports(1u, &m_viewport);
    m_commandList->RSSetScissorRects(1u, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Record commands.
    const float* clearColor = &m_mainPassCBData.FogColor.x;
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);
    m_commandList->OMSetRenderTargets(1u, &rtvHandle, TRUE, &dsvHandle);
    
#pragma region BypassResources
    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    m_commandList->SetGraphicsRootConstantBufferView(1u, currFramePassCB->GetGPUVirtualAddress());

    // Bind all the materials used in this scene. For structured buffers, we can bypass the heap and set as a root descriptor.
    auto matBuffer = m_currFrameResource->MaterialSB->Get();
    m_commandList->SetGraphicsRootShaderResourceView(2u, matBuffer->GetGPUVirtualAddress());

    // Set shaadow map texture for main pass
    m_commandList->SetGraphicsRootDescriptorTable(3u, m_cascadeShadowSrv);

    // Bind all the textures used in this scene. Observe that we only have to specify the first descriptor in the table.  
    // The root signature knows how many descriptors are expected in the table.
    m_commandList->SetGraphicsRootDescriptorTable(4u, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

#pragma endregion BypassResources

    m_commandList->SetPipelineState(m_pipelineStates.at("opaque").Get());
    DrawRenderItems(m_commandList.Get(), m_renderItems);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1u, &transition);

    ThrowIfFailed(m_commandList->Close());
}

void Engine::RenderDepthOnlyPass()
{
    UINT passCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    m_commandList->RSSetViewports(1u, &m_cascadeShadowMap->GetViewport());
    m_commandList->RSSetScissorRects(1u, &m_cascadeShadowMap->GetScissorRect());

    // change to depth write state
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_cascadeShadowMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_commandList->ResourceBarrier(1u, &transition);

    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    m_commandList->SetGraphicsRootConstantBufferView(1u, currFramePassCB->GetGPUVirtualAddress() + passCBByteSize); //cause shadow pass data lies in the second element

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_cascadeShadowMap->GetDsv());
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);
    m_commandList->OMSetRenderTargets(0u, nullptr, TRUE, &dsvHandle);

    m_commandList->SetPipelineState(m_pipelineStates.at("cascades_opaque").Get());
    DrawRenderItems(m_commandList.Get(), m_renderItems);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_cascadeShadowMap->Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    m_commandList->ResourceBarrier(1u, &transition);
}

void Engine::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems)
{
    UINT objCBByteSize = (UINT)ScaldUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto currFrameObjCB = m_currFrameResource->ObjectsCB->Get();       // Get actual ID3D12Resource*

    for (auto& ri : renderItems)
    {
        cmdList->IASetPrimitiveTopology(ri->PrimitiveTopologyType);
        cmdList->IASetVertexBuffers(0u, 1u, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());

        //CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
        //texHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, m_cbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = currFrameObjCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        // set only objects' cbv per item
        cmdList->SetGraphicsRootConstantBufferView(0u, objCBAddress);
        //cmdList->SetGraphicsRootDescriptorTable(3u, texHandle);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1u, ri->StartIndexLocation, ri->BaseVertexLocation, 0u);
    }
}

VOID Engine::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 5> Engine::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0u, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        1u, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        2u, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8u);                              // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(
        3u,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        0.0f,
        16u
        );

    const CD3DX12_STATIC_SAMPLER_DESC shadowComparison(
        4u,
        D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        0.0f,
        16u,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
    );

    return { pointWrap, linearWrap, anisotropicWrap, shadowSampler, shadowComparison };
}

std::pair<XMMATRIX, XMMATRIX> Engine::GetLightSpaceMatrix(const float nearZ, const float farZ)
{
    const auto directionalLight = m_mainPassCBData.Lights[0];

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
    for (UINT i = 0; i < MaxCascades; ++i)
    {
        if (i == 0)
        {
            outMatrices.push_back(GetLightSpaceMatrix(m_camera->GetNearZ(), m_shadowCascadeLevels[i]));
        }
        else if (i < MaxCascades - 1)
        {
            outMatrices.push_back(GetLightSpaceMatrix(m_shadowCascadeLevels[i - 1], m_shadowCascadeLevels[i]));
        }
        else
        {
            outMatrices.push_back(GetLightSpaceMatrix(m_shadowCascadeLevels[i - 1], m_shadowCascadeLevels[i]));
        }
    }
}

void Engine::CreateShadowCascadeSplits()
{
    const float minZ = m_camera->GetNearZ();
    const float maxZ = m_camera->GetFarZ();

    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;

    for (int i = 0; i < MaxCascades; i++)
    {
        float p = (i + 1) / (float)(MaxCascades);
        float log = (float)(minZ * pow(ratio, p));
        float uniform = minZ + range * p;
        float d = 0.95f * (log - uniform) + uniform; // 0.95f - idk, just magic value
        m_shadowCascadeLevels[i] = ((d - minZ) / range) * maxZ;
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