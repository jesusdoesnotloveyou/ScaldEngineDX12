#include "stdafx.h"
#include "Engine.h"
#include "Shapes.h"
#include "Common/ScaldMath.h"

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
    //!!!!!!!!!!!!!!!!!!! problem
    CreateGeometry();
    CreateGeometryMaterials();
    CreateRenderItems();
    CreateFrameResources();
    CreateDescriptorHeaps();
    /*CreateConstantBufferViews();*/
    CreateRootSignature();
    // Create the pipeline state, which includes compiling and loading shaders.
    {
        CreateShaders();
        CreatePSO();
    }

    m_commandList->Close();
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // There is a problem in causing D3D12 ERROR: ID3D12CommandAllocator::LUCPrepareForDestruction: A command allocator 0x0000020DACB147D0:'Unnamed ID3D12CommandAllocator Object' is being reset before previous executions associated with the allocator have completed. [ EXECUTION ERROR #552: COMMAND_ALLOCATOR_SYNC]:
    // I suppouse, there is some fence stuff I have no idea how to handle for now
}

VOID Engine::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];
    
    // Create a descriptor table for Pass CBV.
    /*CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 2u);*/
    
    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1u, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0u, 0u, D3D12_SHADER_VISIBILITY_VERTEX);  // a root descriptor for objects' CBVs.
    slotRootParameter[2].InitAsConstantBufferView(1u, 0u, D3D12_SHADER_VISIBILITY_ALL);     // a root descriptor for objects' materials CBVs.
    //slotRootParameter[2].InitAsDescriptorTable(1u, &cbvTable);
    slotRootParameter[3].InitAsConstantBufferView(2u, 0u, D3D12_SHADER_VISIBILITY_ALL);     // a root descriptor for Pass CBV.

    auto staticSamplers = GetStaticSamplers();

    // Root signature is an array of root parameters
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(4u, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
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

    m_shaders["defaultVS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/VertexShader.hlsl", nullptr, "main", "vs_5_1");
    m_shaders["opaquePS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/PixelShader.hlsl", fogDefines, "main", "ps_5_1");
    m_shaders["shadowGS"] = ScaldUtil::CompileShader(L"./Assets/Shaders/GeometryShader.hlsl", nullptr, "main", "gs_5_1");

    // Define the vertex input layout.
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
    opaquePsoDesc.VS = D3D12_SHADER_BYTECODE({ reinterpret_cast<BYTE*>(m_shaders["defaultVS"]->GetBufferPointer()), m_shaders["defaultVS"]->GetBufferSize() });
    opaquePsoDesc.PS = D3D12_SHADER_BYTECODE({ reinterpret_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer()), m_shaders["opaquePS"]->GetBufferSize() });
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = opaquePsoDesc;
    shadowPsoDesc.VS = D3D12_SHADER_BYTECODE
    {
        
    };
    
    shadowPsoDesc.NumRenderTargets = 0u;


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

    MeshData grid = Shapes::CreateGrid(20.0f, 30.0f, 60u, 40u);

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

    SubmeshGeometry mercurySubmesh;
    mercurySubmesh.IndexCount = (UINT)sphere.indices.size();
    mercurySubmesh.StartIndexLocation = mercuryIndexOffset;
    mercurySubmesh.BaseVertexLocation = mercuryVertexOffset;

    SubmeshGeometry venusSubmesh;
    venusSubmesh.IndexCount = (UINT)sphere.indices.size();
    venusSubmesh.StartIndexLocation = venusIndexOffset;
    venusSubmesh.BaseVertexLocation = venusVertexOffset;

    SubmeshGeometry earthSubmesh;
    earthSubmesh.IndexCount = (UINT)sphere.indices.size();
    earthSubmesh.StartIndexLocation = earthIndexOffset;
    earthSubmesh.BaseVertexLocation = earthVertexOffset;

    SubmeshGeometry marsSubmesh;
    marsSubmesh.IndexCount = (UINT)sphere.indices.size();
    marsSubmesh.StartIndexLocation = marsIndexOffset;
    marsSubmesh.BaseVertexLocation = marsVertexOffset;

    SubmeshGeometry planeSubmesh;
    planeSubmesh.IndexCount = (UINT)grid.indices.size();
    planeSubmesh.StartIndexLocation = planeIndexOffset;
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
    flame0->MatCBIndex = 0;
    flame0->DiffuseSrvHeapIndex = 0;
    //flame0->DiffuseAlbedo = XMFLOAT4(Colors::Gold);
    flame0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    flame0->Roughness = 0.2f;
    flame0->MatTransform = XMMatrixIdentity();

    auto sand0 = std::make_unique<Material>();
    sand0->Name = "sand0";
    sand0->MatCBIndex = 1;
    sand0->DiffuseSrvHeapIndex = 1;
    //sand0->DiffuseAlbedo = XMFLOAT4(Colors::Brown);
    sand0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    sand0->Roughness = 0.1f;
    sand0->MatTransform = XMMatrixIdentity();

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 2;
    stone0->DiffuseSrvHeapIndex = 2;
    //stone0->DiffuseAlbedo = XMFLOAT4(Colors::Orchid);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;
    stone0->MatTransform = XMMatrixIdentity();

    auto ground0 = std::make_unique<Material>();
    ground0->Name = "ground0";
    ground0->MatCBIndex = 3;
    ground0->DiffuseSrvHeapIndex = 3;
    //ground0->DiffuseAlbedo = XMFLOAT4(Colors::Green);
    ground0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    ground0->Roughness = 0.3f;
    ground0->MatTransform = XMMatrixIdentity();

    auto wood0 = std::make_unique<Material>();
    wood0->Name = "wood0";
    wood0->MatCBIndex = 4;
    wood0->DiffuseSrvHeapIndex = 4;
    wood0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    wood0->Roughness = 0.5f;
    wood0->MatTransform = XMMatrixIdentity();
    
    auto iron0 = std::make_unique<Material>();
    iron0->Name = "iron0";
    iron0->MatCBIndex = 5;
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

VOID Engine::CreateRenderItems()
{
    auto sunRenderItem = std::make_unique<RenderItem>();
    sunRenderItem->World = XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    sunRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    sunRenderItem->Geo = m_geometries["solarSystem"].get();
    sunRenderItem->Mat = m_materials["flame0"].get();
    sunRenderItem->ObjCBIndex = 0u;
    sunRenderItem->IndexCount = sunRenderItem->Geo->DrawArgs["sun"].IndexCount;
    sunRenderItem->StartIndexLocation = sunRenderItem->Geo->DrawArgs["sun"].StartIndexLocation;
    sunRenderItem->BaseVertexLocation = sunRenderItem->Geo->DrawArgs["sun"].BaseVertexLocation;

    auto mercuryRenderItem = std::make_unique<RenderItem>();
    mercuryRenderItem->World = XMMatrixScaling(0.25f, 0.25f, 0.25f) * XMMatrixTranslation(2.0f, 0.0f, 0.0f);
    mercuryRenderItem->Geo = m_geometries["solarSystem"].get();
    mercuryRenderItem->Mat = m_materials["sand0"].get();
    mercuryRenderItem->ObjCBIndex = 1u;
    mercuryRenderItem->IndexCount = mercuryRenderItem->Geo->DrawArgs["mercury"].IndexCount;
    mercuryRenderItem->StartIndexLocation = mercuryRenderItem->Geo->DrawArgs["mercury"].StartIndexLocation;
    mercuryRenderItem->BaseVertexLocation = mercuryRenderItem->Geo->DrawArgs["mercury"].BaseVertexLocation;

    auto venusRenderItem = std::make_unique<RenderItem>();
    venusRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(3.0f, 0.0f, 3.0f);
    venusRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    venusRenderItem->Geo = m_geometries["solarSystem"].get();
    venusRenderItem->Mat = m_materials["stone0"].get();
    venusRenderItem->ObjCBIndex = 2u;
    venusRenderItem->IndexCount = venusRenderItem->Geo->DrawArgs["venus"].IndexCount;
    venusRenderItem->StartIndexLocation = venusRenderItem->Geo->DrawArgs["venus"].StartIndexLocation;
    venusRenderItem->BaseVertexLocation = venusRenderItem->Geo->DrawArgs["venus"].BaseVertexLocation;

    auto earthRenderItem = std::make_unique<RenderItem>();
    earthRenderItem->World = XMMatrixScaling(0.6f, 0.6f, 0.6f) * XMMatrixTranslation(4.0f, 0.0f, 4.0f);
    earthRenderItem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
    earthRenderItem->Geo = m_geometries["solarSystem"].get();
    earthRenderItem->Mat = m_materials["ground0"].get();
    earthRenderItem->ObjCBIndex = 3u;
    earthRenderItem->IndexCount = earthRenderItem->Geo->DrawArgs["earth"].IndexCount;
    earthRenderItem->StartIndexLocation = earthRenderItem->Geo->DrawArgs["earth"].StartIndexLocation;
    earthRenderItem->BaseVertexLocation = earthRenderItem->Geo->DrawArgs["earth"].BaseVertexLocation;

    auto marsRenderItem = std::make_unique<RenderItem>();
    marsRenderItem->World = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(5.0f, 0.0f, 5.0f);
    marsRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    marsRenderItem->Geo = m_geometries["solarSystem"].get();
    marsRenderItem->Mat = m_materials["iron0"].get();
    marsRenderItem->ObjCBIndex = 4u;
    marsRenderItem->IndexCount = marsRenderItem->Geo->DrawArgs["mars"].IndexCount;
    marsRenderItem->StartIndexLocation = marsRenderItem->Geo->DrawArgs["mars"].StartIndexLocation;
    marsRenderItem->BaseVertexLocation = marsRenderItem->Geo->DrawArgs["mars"].BaseVertexLocation;

    auto planeRenderItem = std::make_unique<RenderItem>();
    planeRenderItem->World = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, -1.5f, 0.0f);
    //planeRenderItem->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
    planeRenderItem->Geo = m_geometries["solarSystem"].get();
    planeRenderItem->Mat = m_materials["wood0"].get();
    planeRenderItem->ObjCBIndex = 5u;
    planeRenderItem->IndexCount = planeRenderItem->Geo->DrawArgs["plane"].IndexCount;
    planeRenderItem->StartIndexLocation = planeRenderItem->Geo->DrawArgs["plane"].StartIndexLocation;
    planeRenderItem->BaseVertexLocation = planeRenderItem->Geo->DrawArgs["plane"].BaseVertexLocation;

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
        m_frameResources.push_back(std::make_unique<FrameResource>(m_device.Get(), 1u, (UINT)m_renderItems.size(), (UINT)m_materials.size()));
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
    srvHeapDesc.NumDescriptors = (UINT)m_textures.size();
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
}

VOID Engine::CreateConstantBufferViews()
{
    UINT objCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)); // make multiple of 256
    UINT objCount = (UINT)m_renderItems.size();

    for (int frameInd = 0; frameInd < gNumFrameResources; frameInd++)
    {
        auto objectCB = m_frameResources[frameInd]->ObjectsCB->Get();
        for (UINT i = 0; i < objCount; i++)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();
            // offset to ith object cb in the buffer
            cbAddress += i * objCBByteSize;

            // Offset to the object cbv in the descriptor heap.
            int heapIndex = frameInd * objCount + i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, m_cbvSrvUavDescriptorSize);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            m_device->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(PassConstants)); // make multiple of 256
    // only one pass at this moment
    for (int frameInd = 0; frameInd < gNumFrameResources; frameInd++)
    {
        auto passCB = m_frameResources[frameInd]->PassCB->Get();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

        // Offset to the pass cbv in the descriptor heap.
        int heapIndex = m_passCbvOffset + frameInd;
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, m_cbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = passCBByteSize;

        m_device->CreateConstantBufferView(&cbvDesc, handle);
    }
}

VOID Engine::Reset()
{
    D3D12Sample::Reset();

    // Init/Reinit camera
    m_camera->Reset(0.25f * XM_PI, m_aspectRatio, 1.0f, 1000.0f);
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
    UpdateMaterialCB(st);
    UpdatePassCB(st);
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
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - m_lastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - m_lastMousePos.y);

        m_camera->AdjustCameraRadius(dx - dy);
    }

    m_lastMousePos.x = static_cast<float>(x);
    m_lastMousePos.y = static_cast<float>(y);
}

void Engine::OnKeyboardInput(const ScaldTimer& st)
{
    if (GetAsyncKeyState('1') & 0x8000)
        m_isWireframe = true;
    else
        m_isWireframe = false;

    const float dt = st.DeltaTime();

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        m_sunTheta -= 1.0f * dt;

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        m_sunTheta += 1.0f * dt;

    if (GetAsyncKeyState(VK_UP) & 0x8000)
        m_sunPhi -= 1.0f * dt;
    
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        m_sunPhi += 1.0f * dt;

    m_sunPhi = ScaldMath::Clamp(m_sunPhi, 0.1f, XM_PIDIV2);
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
            XMStoreFloat4x4(&m_perObjectConstantBufferData.World, XMMatrixTranspose(ri->World));
            XMStoreFloat4x4(&m_perObjectConstantBufferData.TexTransform, XMMatrixTranspose(ri->TexTransform));
            objectCB->CopyData(ri->ObjCBIndex, m_perObjectConstantBufferData); // In this case ri->ObjCBIndex would be equal to index 'i' of traditional for loop

            ri->NumFramesDirty--;
        }
    }
}

void Engine::UpdatePassCB(const ScaldTimer& st)
{
    XMMATRIX view = m_camera->GetViewMatrix();
    XMMATRIX proj = m_camera->GetPerspectiveProjectionMatrix();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&m_passConstantBufferData.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_passConstantBufferData.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&m_passConstantBufferData.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&m_passConstantBufferData.InvViewProj, XMMatrixTranspose(invViewProj));
    m_passConstantBufferData.EyePosW = m_camera->GetPosition();
    m_passConstantBufferData.NearZ = 1.0f;
    m_passConstantBufferData.FarZ = 1000.0f;
    m_passConstantBufferData.DeltaTime = st.DeltaTime();
    m_passConstantBufferData.TotalTime = st.TotalTime();

    // Invert sign because other way light would be pointing up
    XMVECTOR lightDir = -ScaldMath::SphericalToCarthesian(1.0f, m_sunTheta, m_sunPhi);

    m_passConstantBufferData.Ambient = { 0.25f, 0.25f, 0.35f, 1.0f };

#pragma region DirLights
    XMStoreFloat3(&m_passConstantBufferData.Lights[0].Direction, lightDir);
    m_passConstantBufferData.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };
#pragma endregion DirLights

#pragma region PointLights
    m_passConstantBufferData.Lights[1].Position = {0.0f, 0.0f, 2.0f};
    m_passConstantBufferData.Lights[1].FallOfStart = 5.0f;
    m_passConstantBufferData.Lights[1].FallOfEnd = 10.0f;
    m_passConstantBufferData.Lights[1].Strength = { 0.9f, 0.5f, 0.9f };
#pragma endregion PointLights

    auto currPassCB = m_currFrameResource->PassCB.get();
    currPassCB->CopyData(0, m_passConstantBufferData);
}

void Engine::UpdateMaterialCB(const ScaldTimer& st)
{
    auto materialCB = m_currFrameResource->MaterialCB.get();

    for (auto& e : m_materials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            m_perMaterialConstantBufferData.DiffuseAlbedo = mat->DiffuseAlbedo;
            m_perMaterialConstantBufferData.FresnelR0 = mat->FresnelR0;
            m_perMaterialConstantBufferData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&m_perMaterialConstantBufferData.MatTransform, XMMatrixTranspose(mat->MatTransform));

            materialCB->CopyData(mat->MatCBIndex, m_perMaterialConstantBufferData);

            mat->NumFramesDirty--;
        }
    }
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
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates["opaque"].Get()));
    }

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Access for setting and using root descriptor table
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1u, &transition);

    // The viewport needs to be reset whenever the command list is reset.
    m_commandList->RSSetViewports(1u, &m_viewport);
    m_commandList->RSSetScissorRects(1u, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Record commands.
    const float* clearColor = &m_passConstantBufferData.FogColor.x;
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);
    m_commandList->OMSetRenderTargets(1u, &rtvHandle, TRUE, &dsvHandle);

    // For root descriptor table
    //int passCbvIndex = m_passCbvOffset + m_currFrameResourceIndex;
    //CD3DX12_GPU_DESCRIPTOR_HANDLE passCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    //passCbvHandle.Offset(passCbvIndex, m_cbvSrvUavDescriptorSize);
    //m_commandList->SetGraphicsRootDescriptorTable(/*see root signiture*/1u, passCbvHandle);
    
    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    m_commandList->SetGraphicsRootConstantBufferView(3u, currFramePassCB->GetGPUVirtualAddress());

    DrawRenderItems(m_commandList.Get(), m_renderItems);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1u, &transition);

    ThrowIfFailed(m_commandList->Close());
}

void Engine::RenderDepthOnlyPass(ID3D12GraphicsCommandList* cmdList)
{
    
}

void Engine::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<std::unique_ptr<RenderItem>>& renderItems)
{
    UINT objCount = (UINT)renderItems.size();
    UINT materialCount = (UINT)m_materials.size();

    UINT objCBByteSize = (UINT)ScaldUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT materialCBByteSize = (UINT)ScaldUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto currFrameObjCB = m_currFrameResource->ObjectsCB->Get();       // Get actual ID3D12Resource*
    auto currFrameMaterialCB = m_currFrameResource->MaterialCB->Get(); // Get actual ID3D12Resource*

    for (auto& ri : renderItems)
    {
        cmdList->IASetPrimitiveTopology(ri->PrimitiveTopologyType);
        cmdList->IASetVertexBuffers(0u, 1u, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());

        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
        texHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, m_cbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = currFrameObjCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = currFrameMaterialCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * materialCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0u, texHandle);
        cmdList->SetGraphicsRootConstantBufferView(1u, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(2u, materialCBAddress);

        // The approach to bind cbv using Root Descriptor Table via Handle and cbvHeapStart address
        //CD3DX12_GPU_DESCRIPTOR_HANDLE objCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
        //objCbvHandle.Offset(objCount * m_currFrameResourceIndex + ri->ObjCBIndex, m_cbvSrvUavDescriptorSize);
        //cmdList->SetGraphicsRootDescriptorTable(/*see root signiture*/0u, objCbvHandle);

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

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> Engine::GetStaticSamplers()
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

    return { pointWrap, linearWrap, anisotropicWrap };
}