#include "stdafx.h"
#include "Engine.h"
#include "Shapes.h"

extern const int gNumFrameResources;

Engine::Engine(UINT width, UINT height, std::wstring name, std::wstring className) :
    D3D12Sample(width, height, name, className),
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_dsvDescriptorSize(0)
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
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    CreateDebugLayer();
#endif

    CreateDevice();
    CreateCommandObjects();
    CreateFence();
    CreateRtvAndDsvDescriptorHeaps();
    CreateSwapChain();

    Reset();
}

VOID Engine::CreateDebugLayer()
{
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        // Enable additional debug layers.
        m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
}

VOID Engine::CreateDevice()
{
    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        GetHardwareAdapter(m_factory.Get(), &m_hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }
}

VOID Engine::CreateCommandObjects()
{
    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[m_frameIndex])));

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(
        0u /*Single GPU*/,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get() /*Must match the command list type*/,
        nullptr,
        IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());
}

VOID Engine::CreateFence()
{
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        
        m_fenceValues[m_frameIndex]++;
        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForGPU();
    }
}

VOID Engine::CreateRtvAndDsvDescriptorHeaps()
{
    // Create descriptor heaps.
    // Descriptor heap has to be created for every GPU resource

    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1u;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0u;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

VOID Engine::CreateSwapChain()
{
    // Describe and create the swap chain.
    //DXGI_SWAP_CHAIN_DESC sd;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = BackBufferFormat; // Back buffer format
    swapChainDesc.SampleDesc.Count = 1u; // MSAA
    swapChainDesc.SampleDesc.Quality = 0u; // MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullScreenDesc = {};
    swapChainFullScreenDesc.RefreshRate.Numerator = 60u;
    swapChainFullScreenDesc.RefreshRate.Denominator = 1u;
    swapChainFullScreenDesc.Windowed = TRUE;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32App::GetHwnd(),
        &swapChainDesc,
        &swapChainFullScreenDesc,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(m_factory->MakeWindowAssociation(Win32App::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

// Load the sample assets.
VOID Engine::LoadAssets()
{
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
    
    //!!!!!!!!!!!!!!!!!!! problem
    CreateGeometry();
    CreateGeometryMaterials();
    CreateRenderItems();
    CreateFrameResources();
    /*CreateDescriptorHeaps();
    CreateConstantBufferViews();*/
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
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    // Create a root descriptor for objects' CBVs.
    CD3DX12_ROOT_DESCRIPTOR objCbvRootDesc;
    objCbvRootDesc.Init(0u);

    // Create a root descriptor for objects' CBVs.
    CD3DX12_ROOT_DESCRIPTOR matCbvRootDesc;
    matCbvRootDesc.Init(1u);
    
    // Create a descriptor table for Pass CBV.
    /*CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 2u);*/

    // Create a root descriptor for Pass CBV.
    CD3DX12_ROOT_DESCRIPTOR passCbvRootDesc;
    passCbvRootDesc.Init(2u);
    
    slotRootParameter[0].InitAsConstantBufferView(0u, 0u, D3D12_SHADER_VISIBILITY_VERTEX);
    slotRootParameter[1].InitAsConstantBufferView(1u, 0u, D3D12_SHADER_VISIBILITY_VERTEX);
    //slotRootParameter[2].InitAsDescriptorTable(1u, &cbvTable);
    slotRootParameter[2].InitAsConstantBufferView(2u, 0u, D3D12_SHADER_VISIBILITY_VERTEX);

    // Root signature is an array of root parameters
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(3u, slotRootParameter, 0u, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> signature = nullptr;
    ComPtr<ID3DBlob> error = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0u, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

VOID Engine::CreateShaders()
{
#if defined(_DEBUG) | defined(DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    //auto pixelShaderPath = GetAssetFullPath(L"./PixelShader.hlsl").c_str();

    ComPtr<ID3DBlob> pErrorMsgVS;
    ComPtr<ID3DBlob> pErrorMsgPS;

    ThrowIfFailed(D3DCompileFromFile(L"./Assets/Shaders/VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_1", compileFlags, 0u, &m_vertexShader, &pErrorMsgVS));
    ThrowIfFailed(D3DCompileFromFile(L"./Assets/Shaders/PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_1", compileFlags, 0u, &m_pixelShader, &pErrorMsgPS));

    // Define the vertex input layout.
    m_inputLayout =
    {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
        { "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 12u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
    };
}

VOID Engine::CreatePSO()
{
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    opaquePsoDesc.pRootSignature = m_rootSignature.Get();
    opaquePsoDesc.VS = D3D12_SHADER_BYTECODE({ reinterpret_cast<BYTE*>(m_vertexShader->GetBufferPointer()), m_vertexShader->GetBufferSize() });
    opaquePsoDesc.PS = D3D12_SHADER_BYTECODE({ reinterpret_cast<BYTE*>(m_pixelShader->GetBufferPointer()), m_pixelShader->GetBufferSize() });
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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

    // ???
    m_commandList->SetPipelineState(m_pipelineStates["opaque"].Get());
}

VOID Engine::CreateGeometry()
{
    // Just meshes
    Shapes::MeshData box = Shapes::CreateBox(1.0f, 1.0f, 1.0f);
    // the same mesh for all planets
    Shapes::MeshData sphere = Shapes::CreateSphere(1.0f, 16u, 16u);

    // Create shared vertex/index buffer for all geometry.
    UINT sunVertexOffset = 0u;
    UINT mercuryVertexOffset = sunVertexOffset + (UINT)sphere.vertices.size();
    UINT venusVertexOffset = mercuryVertexOffset + (UINT)sphere.vertices.size();
    UINT earthVertexOffset = venusVertexOffset + (UINT)sphere.vertices.size();

    UINT sunIndexOffset = 0u;
    UINT mercuryIndexOffset = sunIndexOffset + (UINT)sphere.indices.size();
    UINT venusIndexOffset = mercuryIndexOffset + (UINT)sphere.indices.size();
    UINT earthIndexOffset = venusIndexOffset + (UINT)sphere.indices.size();

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

    auto totalVertexCount = 
        sphere.vertices.size() // sun
        + sphere.vertices.size() // merc
        + sphere.vertices.size() // venus
        + sphere.vertices.size() //earth
        ;
    auto totalIndexCount = 
        sphere.indices.size()
        + sphere.indices.size()
        + sphere.indices.size()
        + sphere.indices.size()
        ;

    std::vector<Vertex> vertices(totalVertexCount);

    int k = 0;
    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
    }

    for (size_t i = 0; i < sphere.vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.vertices[i].position;
        vertices[k].normal = sphere.vertices[i].normal;
    }

    std::vector<uint16_t> indices;
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());
    indices.insert(indices.end(), sphere.indices.begin(), sphere.indices.end());

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

    m_geometries[geometry->Name] = std::move(geometry);
}

VOID Engine::CreateGeometryMaterials()
{
    auto flame0 = std::make_unique<Material>();
    flame0->Name = "flame0";
    flame0->MatCBIndex = 0;
    flame0->DiffuseAlbedo = XMFLOAT4(Colors::Gold);
    flame0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    flame0->Roughness = 0.2f;

    auto sand0 = std::make_unique<Material>();
    sand0->Name = "sand0";
    sand0->MatCBIndex = 1;
    sand0->DiffuseAlbedo = XMFLOAT4(Colors::Brown);
    sand0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    sand0->Roughness = 0.1f;

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 2;
    stone0->DiffuseAlbedo = XMFLOAT4(Colors::Orchid);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;

    auto ground0 = std::make_unique<Material>();
    ground0->Name = "ground0";
    ground0->MatCBIndex = 3;
    ground0->DiffuseAlbedo = XMFLOAT4(Colors::Green);
    ground0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05);
    ground0->Roughness = 0.3f;

    m_materials["flame0"] = std::move(flame0);
    m_materials["sand0"] = std::move(sand0);
    m_materials["stone0"] = std::move(stone0);
    m_materials["ground0"] = std::move(ground0);
}

VOID Engine::CreateRenderItems()
{
    auto sunRenderItem = std::make_unique<RenderItem>();
    sunRenderItem->World = XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
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
    venusRenderItem->Geo = m_geometries["solarSystem"].get();
    venusRenderItem->Mat = m_materials["stone0"].get();
    venusRenderItem->ObjCBIndex = 2u;
    venusRenderItem->IndexCount = venusRenderItem->Geo->DrawArgs["venus"].IndexCount;
    venusRenderItem->StartIndexLocation = venusRenderItem->Geo->DrawArgs["venus"].StartIndexLocation;
    venusRenderItem->BaseVertexLocation = venusRenderItem->Geo->DrawArgs["venus"].BaseVertexLocation;

    auto earthRenderItem = std::make_unique<RenderItem>();
    earthRenderItem->World = XMMatrixScaling(0.6f, 0.6f, 0.6f) * XMMatrixTranslation(4.0f, 0.0f, 4.0f);
    earthRenderItem->Geo = m_geometries["solarSystem"].get();
    earthRenderItem->Mat = m_materials["ground0"].get();
    earthRenderItem->ObjCBIndex = 3u;
    earthRenderItem->IndexCount = earthRenderItem->Geo->DrawArgs["earth"].IndexCount;
    earthRenderItem->StartIndexLocation = earthRenderItem->Geo->DrawArgs["earth"].StartIndexLocation;
    earthRenderItem->BaseVertexLocation = earthRenderItem->Geo->DrawArgs["earth"].BaseVertexLocation;

    m_renderItems.push_back(std::move(sunRenderItem));
    m_renderItems.push_back(std::move(mercuryRenderItem));
    m_renderItems.push_back(std::move(venusRenderItem));
    m_renderItems.push_back(std::move(earthRenderItem));

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

    m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
    // Before making any changes
    //FlushCommandQueue();

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

    for (UINT i = 0; i < FrameCount; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencilBuffer.Reset();

    // Resize the swap chain
    ThrowIfFailed(m_swapChain->ResizeBuffers(
        FrameCount,
        m_width,
        m_height,
        BackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    m_frameIndex = 0u;

    // Create/recreate frame resources.
    {
        // Create Render Targets
        // Start of the rtv heap
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < FrameCount; i++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, m_rtvDescriptorSize);

            // Create command allocator for every back buffer
            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        }

        // Create the depth/stencil view.
        D3D12_RESOURCE_DESC depthStencilDesc;
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = m_width;
        depthStencilDesc.Height = m_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; //
        // MSAA, same settings as back buffer
        depthStencilDesc.SampleDesc.Count = 1u;
        depthStencilDesc.SampleDesc.Quality = 0u;

        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = DepthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0u;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /* Once created and never changed */),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DepthStencilFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0u;

        m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);
    }

    // Transition the resource from its initial state to be used as a depth buffer.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Execute the resize commands. 
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Wait until resize is complete.
    //FlushCommandQueue();

    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<FLOAT>(m_width);
    m_viewport.Height = static_cast<FLOAT>(m_height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(m_width);
    m_scissorRect.bottom = static_cast<LONG>(m_height);

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
    mLastMousePos.x = static_cast<float>(x);
    mLastMousePos.y = static_cast<float>(y);

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
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        m_camera->AdjustYaw(dx); 
        m_camera->AdjustPitch(dy);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        m_camera->AdjustCameraRadius(dx - dy);
    }

    mLastMousePos.x = static_cast<float>(x);
    mLastMousePos.y = static_cast<float>(y);
}

void Engine::OnKeyboardInput(const ScaldTimer& st)
{
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
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
    m_passConstantBufferData.NearZ = 1.0f;
    m_passConstantBufferData.FarZ = 1000.0f;
    m_passConstantBufferData.DeltaTime = st.DeltaTime();
    m_passConstantBufferData.TotalTime = st.TotalTime();

    auto passCB = m_currFrameResource->PassCB.get();
    passCB->CopyData(0, m_passConstantBufferData);
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

            materialCB->CopyData(mat->MatCBIndex, m_perMaterialConstantBufferData);

            mat->NumFramesDirty--;
        }
    }
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

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = currFrameObjCB->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = currFrameMaterialCB->GetGPUVirtualAddress();
        
        cmdList->SetGraphicsRootConstantBufferView(0u, objCBAddress + ri->ObjCBIndex * objCBByteSize);
        cmdList->SetGraphicsRootConstantBufferView(1u, materialCBAddress + ri->Mat->MatCBIndex * materialCBByteSize);

        // The approach to bind cbv using Root Descriptor Table via Handle and cbvHeapStart address
        //CD3DX12_GPU_DESCRIPTOR_HANDLE objCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
        //objCbvHandle.Offset(objCount * m_currFrameResourceIndex + ri->ObjCBIndex, m_cbvSrvUavDescriptorSize);
        //cmdList->SetGraphicsRootDescriptorTable(/*see root signiture*/0u, objCbvHandle);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1u, ri->StartIndexLocation, ri->BaseVertexLocation, 0u);
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
    if (mIsWireframe)
    {
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(m_commandList->Reset(currentCmdAlloc, m_pipelineStates["opaque"].Get()));
    }

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // For root descriptor table
    /*ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);*/

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1u, &transition);

    // The viewport needs to be reset whenever the command list is reset.
    m_commandList->RSSetViewports(1u, &m_viewport);
    m_commandList->RSSetScissorRects(1u, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);
    m_commandList->OMSetRenderTargets(1u, &rtvHandle, TRUE, &dsvHandle);

    // For root descriptor table
    //int passCbvIndex = m_passCbvOffset + m_currFrameResourceIndex;
    //CD3DX12_GPU_DESCRIPTOR_HANDLE passCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    //passCbvHandle.Offset(passCbvIndex, m_cbvSrvUavDescriptorSize);
    //m_commandList->SetGraphicsRootDescriptorTable(/*see root signiture*/1u, passCbvHandle);
    
    auto currFramePassCB = m_currFrameResource->PassCB->Get();
    m_commandList->SetGraphicsRootConstantBufferView(2u, currFramePassCB->GetGPUVirtualAddress());

    DrawRenderItems(m_commandList.Get(), m_renderItems);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1u, &transition);

    ThrowIfFailed(m_commandList->Close());
}

// Wait for pending GPU work to complete.
VOID Engine::WaitForGPU()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));
   
    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    m_fenceValues[m_frameIndex]++;
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