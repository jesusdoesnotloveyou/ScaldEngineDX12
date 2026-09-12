#include "ShadowMap.h"
#include "Device.h"

using namespace Scald;

ShadowMap::ShadowMap(Device* device, UINT width, UINT height, UINT cascadesCount)
    : m_device(device),
      m_mapWidth(width),
      m_mapHeight(height),
      m_cascadesCount(cascadesCount)
{
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<FLOAT>(width);
    m_viewport.Height = static_cast<FLOAT>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = 0L;
    m_scissorRect.top = 0L;
    m_scissorRect.right = static_cast<LONG>(width);
    m_scissorRect.bottom = static_cast<LONG>(height);

    if (m_cascadesCount == 0)
    {
        CreateResource();
    }
}

ShadowMap::~ShadowMap() noexcept {}

ID3D12Resource* ShadowMap::Get()
{
    return m_shadowMap.Get();
}

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMap::GetGpuSrv() const
{
    return m_srvAllocation.GetGpuHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDsv() const
{
    return m_dsvAllocation.GetCpuHandle();
}

void ShadowMap::CreateDescriptors()
{
    m_dsvAllocation = m_device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u);             // Handle for binding shadow map as DSV at the Z-Prepass
    m_srvAllocation = m_device->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1u); // Handle for binding shadow map to shader program

    CreateViews();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
    if ((m_mapWidth != newWidth) || (m_mapHeight != newHeight))
    {
        m_mapWidth = newWidth;
        m_mapHeight = newHeight;

        CreateResource();
        // New resource, so we need new descriptors to that resource.
        CreateViews();
    }
}

void ShadowMap::CreateViews()
{
    // Create SRV to resource so we can sample the shadow map in a shader program.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0u;
    m_device->GetD3D12Device()->CreateShaderResourceView(m_shadowMap.Get(), &srvDesc, m_srvAllocation.GetCpuHandle());

    // Create DSV to resource so we can render to the shadow map.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    ZeroMemory(&dsvDesc, sizeof(dsvDesc));
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0u;
    m_device->GetD3D12Device()->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, m_dsvAllocation.GetCpuHandle());
}

void ShadowMap::CreateShadowCascadeSplits(float nearZ, float farZ)
{
    const float minZ = nearZ;
    const float maxZ = farZ;

    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;

    for (int i = 0; i < MaxCascades; i++)
    {
        float p = (i + 1) / (float)(MaxCascades);
        float log = (float)(minZ * pow(ratio, p));
        float uniform = minZ + range * p;
        float d = 0.95f * (log - uniform) + uniform;  // 0.95f - idk, just magic value
        m_shadowCascadeLevels[i] = ((d - minZ) / range) * maxZ;
    }
}

void ShadowMap::CreateResource()
{
    D3D12_RESOURCE_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(D3D12_RESOURCE_DESC));

    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = (UINT64)0;
    textureDesc.Width = (UINT64)m_mapWidth;
    textureDesc.Height = m_mapHeight;
    textureDesc.DepthOrArraySize = (UINT16)1;
    textureDesc.MipLevels = (UINT16)1;
    textureDesc.Format = m_format;
    textureDesc.SampleDesc.Count = 1u;
    textureDesc.SampleDesc.Quality = 0u;

    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = (UINT8)0;

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(m_device->GetD3D12Device()->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_shadowMap)));

    m_shadowMap->SetName(m_cascadesCount != 0u ? L"CascadedShadowMap" : L"ShadowMap");
}