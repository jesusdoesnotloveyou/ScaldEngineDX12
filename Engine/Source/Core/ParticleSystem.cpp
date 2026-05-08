#include "stdafx.h"
#include "ParticleSystem.h"

#include "Common/Camera.h"

// The number of elements to sort is limited to an even power of 2
// At minimum 8,192 elements - BITONIC_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE
// At maximum 262,144 elements - BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE

const UINT NUM_ELEMENTS = 512 * 512;
const UINT BITONIC_BLOCK_SIZE = 512;
const UINT TRANSPOSE_BLOCK_SIZE = 16;
const UINT MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
const UINT MATRIX_HEIGHT = NUM_ELEMENTS / BITONIC_BLOCK_SIZE;

ParticleSystem::ParticleSystem(ID3D12Device* device, int maxParticles, XMVECTOR origin, class Camera* camera)
    : m_device(device),
      maxParticles(maxParticles),
      origin(origin),
      camera(camera)
{
    // ThrowIfFailed(CreateWICTextureFromFile(mDevice, L"./Data/Textures/pop_cat_color.png", nullptr, &mBillboardTexture));
}

void ParticleSystem::Render()
{
    // mDeviceContext->PSSetShaderResources(0u, 1u, mBillboardTexture.GetAddressOf());
    // mDeviceContext->PSSetSamplers(0u, 1u, mParticleSamplerClamp.GetAddressOf());
}

void ParticleSystem::Emit(int numParticles)
{
    for (int i = 0; i < numParticles; i++)
    {
        InitializeParticle(i);
    }
}

void ParticleSystem::InitializeSystem()
{
    ThrowIfFailed(mBillboardGeometryShader.Init(m_device, L"./Shaders/particles/BillboardParticleGS.hlsl"));
    ThrowIfFailed(mBitonicSortShader.Init(m_device, L"./Shaders/particles/SortCS.hlsl"));
    ThrowIfFailed(mBitonicTransposeShader.Init(m_device, L"./Shaders/particles/SortTransposeCS.hlsl"));

    ThrowIfFailed(mCBSort.Init(m_device, mDeviceContext));
    ThrowIfFailed(mCBParticle.Init(m_device, mDeviceContext));
    ThrowIfFailed(mCBCamera.Init(m_device, mDeviceContext));

    // Injection buffer
    ThrowIfFailed(CreateRWStructuredBufferCPUWrite<Particle>(m_device, &injectionBuffer, (UINT)injectionBufferSize));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = (UINT64)0u;
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);
    srvDesc.Buffer.NumElements = injectionBufferSize;
    m_device->CreateShaderResourceView(injectionBuffer.Get(), &srvDesc, mInjectionBufferSRV);

    // indirect args buffer
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = 16u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    desc.CPUAccessFlags = 0u;

    D3D11_DRAW_INSTANCED_INDIRECT_ARGS args = {};
    args.VertexCountPerInstance = 0u;
    args.InstanceCount = 1u;
    args.StartVertexLocation = 0u;
    args.StartInstanceLocation = 0u;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = &args;
    ThrowIfFailed(m_device->CreateBuffer(&desc, &srd, &indirectArgsBuffer));

    // Main particle pool
    ThrowIfFailed(CreateRWStructuredBuffer<Particle>(m_device, &particleBuffer, maxParticles));

    D3D12_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
    particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    particleUAVDesc.Buffer.NumElements = maxParticles;
    particleUAVDesc.Buffer.FirstElement = 0u;
    particleUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_device->CreateUnorderedAccessView(particleBuffer.Get(), nullptr, &particleUAVDesc, mParticleBufferUAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
    particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    particleSRVDesc.Buffer.NumElements = maxParticles;
    particleSRVDesc.Buffer.FirstElement = 0u;
    m_device->CreateShaderResourceView(particleBuffer.Get(), &particleSRVDesc, mParticleBufferSRV);

    // Vectors for sort list and dead list
    std::vector<SortList> sort(maxParticles);
    std::vector<UINT> deadIndeces(maxParticles);

    for (UINT i = 0; i < maxParticles; ++i)
    {
        sort[i] = {i, std::numeric_limits<float>().max()};
        deadIndeces[i] = i;
    }
    mParticleData.numAliveParticles = 0u;

    // Alive (sort) lists
    ThrowIfFailed(CreateRWStructuredBuffer<SortList>(m_device, &sortListBuffer, maxParticles, sort.data()));
    ThrowIfFailed(CreateRWStructuredBuffer<SortList>(m_device, &sortListBuffer2, maxParticles, sort.data()));

    D3D12_UNORDERED_ACCESS_VIEW_DESC sortListUAVDesc;
    sortListUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortListUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    sortListUAVDesc.Buffer.FirstElement = 0u;
    sortListUAVDesc.Buffer.NumElements = maxParticles;
    sortListUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;  // was D3D11_BUFFER_UAV_FLAG_COUNTER
    m_device->CreateUnorderedAccessView(sortListBuffer, nullptr, &sortListUAVDesc, mSortListBufferUAV);
    m_device->CreateUnorderedAccessView(sortListBuffer2, nullptr, &sortListUAVDesc, mSortListBufferUAV2);

    D3D12_SHADER_RESOURCE_VIEW_DESC sortListSRVDesc;
    sortListSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortListSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    sortListSRVDesc.Buffer.FirstElement = 0u;
    sortListSRVDesc.Buffer.NumElements = maxParticles;
    m_device->CreateShaderResourceView(sortListBuffer, &sortListSRVDesc, mSortListBufferSRV);
    m_device->CreateShaderResourceView(sortListBuffer2, &sortListSRVDesc, mSortListBufferSRV2);

    // Dead list
    ThrowIfFailed(CreateRWStructuredBuffer<UINT>(m_device, &deadListBuffer, maxParticles, deadIndeces.data()));

    D3D12_UNORDERED_ACCESS_VIEW_DESC deadListUAVDesc;
    deadListUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    deadListUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    deadListUAVDesc.Buffer.FirstElement = 0u;
    deadListUAVDesc.Buffer.NumElements = maxParticles;
    deadListUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_device->CreateUnorderedAccessView(deadListBuffer, nullptr, &deadListUAVDesc, mDeadListBufferUAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC deadListSRVDesc;
    deadListSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    deadListSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    deadListSRVDesc.Buffer.FirstElement = 0u;
    deadListSRVDesc.Buffer.NumElements = maxParticles;
    m_device->CreateShaderResourceView(deadListBuffer, &deadListSRVDesc, mDeadListBufferSRV);
}

ParticleSystem::~ParticleSystem() noexcept
{
    SAFE_RELEASE(particleBuffer);
    SAFE_RELEASE(sortListBuffer);
    SAFE_RELEASE(sortListBuffer2);
    SAFE_RELEASE(deadListBuffer);
    SAFE_RELEASE(injectionBuffer);
    SAFE_RELEASE(indirectArgsBuffer);
}

void ParticleSystem::SortParticles()
{
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    const UINT MATRIX_HEIGHT = maxParticles / BITONIC_BLOCK_SIZE;

    // Sort the data
    // First sort the rows for the levels <= to the block size
    for (UINT level = 2; level <= BITONIC_BLOCK_SIZE; level = level * 2)
    {
        SetConstBuffer(level, level, MATRIX_HEIGHT, MATRIX_WIDTH);

        // Sort the row data
        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mSortListBufferUAV.GetAddressOf(), nullptr);
        mDeviceContext->CSSetShader(mBitonicSortShader.Get(), nullptr, 0u);

        if (maxParticles / BITONIC_BLOCK_SIZE > 0u) mDeviceContext->Dispatch(maxParticles / BITONIC_BLOCK_SIZE, 1u, 1u);
    }

    // Then sort the rows and columns for the levels > than the block size
    // Transpose. Sort the Columns. Transpose. Sort the Rows.
    for (UINT level = (BITONIC_BLOCK_SIZE * 2); level <= maxParticles; level = level * 2)
    {
        SetConstBuffer(level / BITONIC_BLOCK_SIZE, (level & ~maxParticles) / BITONIC_BLOCK_SIZE, MATRIX_WIDTH, MATRIX_HEIGHT);

        // Transpose the data from buffer 1 into buffer 2
        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mSortListBufferUAV.GetAddressOf(), nullptr);
        mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, mSortListBufferUAV2.GetAddressOf(), nullptr);
        mDeviceContext->CSSetShader(mBitonicTransposeShader.Get(), nullptr, 0u);

        if (MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE > 0u && MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE > 0u)
            mDeviceContext->Dispatch(MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, 1u);
        mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, &nullUAV, nullptr);

        // Sort the transposed column data
        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mSortListBufferUAV2.GetAddressOf(), nullptr);
        mDeviceContext->CSSetShader(mBitonicSortShader.Get(), nullptr, 0u);
        if (maxParticles / BITONIC_BLOCK_SIZE > 0u) mDeviceContext->Dispatch(maxParticles / BITONIC_BLOCK_SIZE, 1u, 1u);

        SetConstBuffer(BITONIC_BLOCK_SIZE, level, MATRIX_HEIGHT, MATRIX_WIDTH);

        // Transpose the data from buffer 2 back into buffer 1
        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mSortListBufferUAV2.GetAddressOf(), nullptr);
        mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, mSortListBufferUAV.GetAddressOf(), nullptr);
        mDeviceContext->CSSetShader(mBitonicTransposeShader.Get(), nullptr, 0u);
        if (MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE > 0 && MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE > 0u)
            mDeviceContext->Dispatch(MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, 1u);
        mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, &nullUAV, nullptr);

        // Sort the row data
        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mSortListBufferUAV.GetAddressOf(), nullptr);
        mDeviceContext->CSSetShader(mBitonicSortShader.Get(), nullptr, 0u);
        if (maxParticles / BITONIC_BLOCK_SIZE > 0u) mDeviceContext->Dispatch(maxParticles / BITONIC_BLOCK_SIZE, 1u, 1u);

        mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
    }

    mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void ParticleSystem::SetConstBuffer(UINT iLevel, UINT iLevelMask, UINT iWidth, UINT iHeight)
{
    mSortData = {iLevel, iLevelMask, iWidth, iHeight};
    mCBSort.SetAndApplyData(mSortData);
    mDeviceContext->CSSetConstantBuffers(0u, 1u, mCBSort.GetAddressOf());
}

//--------------------------------------------------------------------------------------
// Create Structured Buffer
//--------------------------------------------------------------------------------------
HRESULT ParticleSystem::CreateStructuredBuffer(ID3D11Device* pDevice, UINT uElementSize, UINT uCount, void* pInitData, ID3D11Buffer** ppBufOut)
{
    *ppBufOut = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = uElementSize * uCount;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = uElementSize;

    if (pInitData)
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        return pDevice->CreateBuffer(&desc, &InitData, ppBufOut);
    }
    else
        return pDevice->CreateBuffer(&desc, nullptr, ppBufOut);
}

//--------------------------------------------------------------------------------------
// Create Raw Buffer
//--------------------------------------------------------------------------------------
HRESULT ParticleSystem::CreateRawBuffer(ID3D11Device* pDevice, UINT uSize, void* pInitData, ID3D11Buffer** ppBufOut)
{
    *ppBufOut = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER;
    desc.ByteWidth = uSize;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    if (pInitData)
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        return pDevice->CreateBuffer(&desc, &InitData, ppBufOut);
    }
    else
        return pDevice->CreateBuffer(&desc, nullptr, ppBufOut);
}

//--------------------------------------------------------------------------------------
// Create Shader Resource View for Structured or Raw Buffers
//--------------------------------------------------------------------------------------
HRESULT ParticleSystem::CreateBufferSRV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11ShaderResourceView** ppSRVOut)
{
    D3D11_BUFFER_DESC descBuf = {};
    pBuffer->GetDesc(&descBuf);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    desc.BufferEx.FirstElement = 0;

    if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        desc.BufferEx.NumElements = descBuf.ByteWidth / 4;
    }
    else if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        // This is a Structured Buffer

        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
    }
    else
    {
        return E_INVALIDARG;
    }

    return pDevice->CreateShaderResourceView(pBuffer, &desc, ppSRVOut);
}

//--------------------------------------------------------------------------------------
// Create Unordered Access View for Structured or Raw Buffers
//--------------------------------------------------------------------------------------
HRESULT ParticleSystem::CreateBufferUAV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11UnorderedAccessView** ppUAVOut)
{
    D3D11_BUFFER_DESC descBuf = {};
    pBuffer->GetDesc(&descBuf);

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;

    if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS;  // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = descBuf.ByteWidth / 4;
    }
    else if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        // This is a Structured Buffer

        desc.Format = DXGI_FORMAT_UNKNOWN;  // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
        desc.Buffer.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
    }
    else
    {
        return E_INVALIDARG;
    }

    return pDevice->CreateUnorderedAccessView(pBuffer, &desc, ppUAVOut);
}