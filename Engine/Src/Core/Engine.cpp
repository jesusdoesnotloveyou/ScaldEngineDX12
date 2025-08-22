#include "stdafx.h"
#include "Engine.h"

Engine::Engine(UINT width, UINT height, std::wstring name, std::wstring className) :
    D3D12Sample(width, height, name, className),
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_dsvDescriptorSize(0),
    m_cbvSrvDescriptorSize(0)
{
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<FLOAT>(width);
    m_viewport.Height = static_cast<FLOAT>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(width);
    m_scissorRect.bottom = static_cast<LONG>(height);
}

Engine::~Engine()
{
    if (m_device)
    {
        FlushCommandQueue();
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
    CreateDescriptorHeaps();
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

VOID Engine::CreateDescriptorHeaps()
{
    // Create descriptor heaps.
    // Descriptor heap has to be created for every GPU resource
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1u;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0u;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 1u;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.NumDescriptors = 1u;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.NodeMask = 0u;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));

        // useless probably
        //m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
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
    // Create an empty root signature.
    CreateRootSignature();

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        CreateShaders();
        CreatePSO();
    }

    // Create the vertex buffer.
    {
        // Define the geometry for a cube.
        Vertex vertices[] =
        {
            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) },
            { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) },
            { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) },
            { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) },
            { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) },
            { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) },
            { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) },
            { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) }
        };

        const UINT64 vbByteSize = ARRAYSIZE(vertices) * sizeof(Vertex);
        ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
        m_vertexBuffer = ScaldUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), vertices, vbByteSize, VertexBufferUploader);
        
        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = vbByteSize;
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);

        // What the fuck?
        // Copy the triangle data to the vertex buffer.
        //UINT8* pVertexDataBegin;
        //CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        //ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        //memcpy(pVertexDataBegin, vertices, sizeof(vertices));
        //m_vertexBuffer->Unmap(0, nullptr);
    }

    // Create the index buffer.
    {
        std::uint16_t indices[] =
        {
            0, 1, 2,
            0, 2, 3,

            4, 6, 5,
            4, 7, 6,

            4, 5, 1,
            4, 1, 0,

            3, 2, 6,
            3, 6, 7,

            1, 5, 6,
            1, 6, 2,

            4, 0, 3,
            4, 3, 7
        };

        const UINT ibByteSize = ARRAYSIZE(indices) * sizeof(uint16_t);
        ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;
        m_indexBuffer = ScaldUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indices, ibByteSize, IndexBufferUploader);
        
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = ibByteSize;
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }
}

VOID Engine::CreateRootSignature()
{
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 0u);
    slotRootParameter[0].InitAsDescriptorTable(1u, &cbvTable);
    
    // Root signature is an array of root parameters
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(1u, slotRootParameter, 0u, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

    ThrowIfFailed(D3DCompileFromFile(L"./Assets/Shaders/VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_1", compileFlags, 0u, &m_vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"./Assets/Shaders/PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_1", compileFlags, 0u, &m_pixelShader, nullptr));

    // Define the vertex input layout.
    m_inputLayout =
    {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
        { "COLOR", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 12u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
    };
}

VOID Engine::CreatePSO()
{
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = D3D12_SHADER_BYTECODE({ reinterpret_cast<BYTE*>(m_vertexShader->GetBufferPointer()), m_vertexShader->GetBufferSize() });
    psoDesc.PS = D3D12_SHADER_BYTECODE({ m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() });
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size()};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1u;
    psoDesc.RTVFormats[0] = BackBufferFormat;
    //psoDesc.DSVFormat = DepthStencilFormat;
    // Do not use multisampling
    // This should match the setting of the render target we are using (check swapChainDesc)
    psoDesc.SampleDesc.Count = 1u;
    psoDesc.SampleDesc.Quality = 0u;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

VOID Engine::CreateConstantBuffer()
{
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(m_device.Get(), 1u, TRUE);
    UINT objCBByteSize = ScaldUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)); // make multiple of 256

    D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = mObjectCB->Get()->GetGPUVirtualAddress();

    // Offset to the ith object constant buffer in the buffer. Here our i = 0.
    int geometryCBufIndex = 0;
    cbvAddress += geometryCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = cbvAddress;
    cbvDesc.SizeInBytes = objCBByteSize;

    m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

VOID Engine::UpdateConstantBuffer()
{

}

std::vector<UINT8> Engine::GenerateTextureData()
{
    return std::vector<UINT8>();
}

VOID Engine::Reset()
{
    // Before making any changes
    FlushCommandQueue();

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
        optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /* Once created and never changed */),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(&m_depthStencilBuffer)));

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //
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
    FlushCommandQueue();

    mProj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, m_aspectRatio, 1.0f, 1000.0f);
}

VOID Engine::FlushCommandQueue()
{
    
}

// Update frame-based values.
void Engine::OnUpdate(const ScaldTimer& st)
{
    // Convert Spherical to Cartesian
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float y = mRadius * sinf(mPhi) * sinf(mTheta);
    float z = mRadius * cosf(mPhi);

    // View matrix
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    mView = XMMatrixLookAtLH(pos, target, up);


    XMMATRIX worldViewProj = mWorld * mView * mProj;

    // Update constant buffer with the lates wvp matrix
    XMStoreFloat4x4(&m_constantBufferData.gWorldViewProj, XMMatrixTranspose(worldViewProj));
    
    // mObjectCB isn't initialized yet
    //mObjectCB->CopyData(0, m_constantBufferData);
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

        mTheta += dx;
        mPhi += dy;

        mPhi = Clamp(mPhi, 0.1f, XM_PI - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        mRadius += dx - dy;

        mRadius = Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = static_cast<float>(x);
    mLastMousePos.y = static_cast<float>(y);
}

void D3D12Sample::Resize()
{
    mAppPaused = true;
    mResizing = true;
    mTimer.Stop();
}

void D3D12Sample::OnResize()
{
    mAppPaused = false;
    mResizing = false;
    mTimer.Start();
}

VOID Engine::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated command lists have finished execution on the GPU;
    // apps should use fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command list,
    // that command list can then be reset at any time and must be before re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
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
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0u, 0u, nullptr);
    m_commandList->OMSetRenderTargets(1u, &rtvHandle, TRUE, &dsvHandle);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0u, 1u, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    m_commandList->SetGraphicsRootDescriptorTable(0u, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    /* Box Geometry */
    m_commandList->DrawIndexedInstanced(8u, 1u, 0u, 0, 0u);

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