#include "GBuffer.h"
#include "Device.h"

using namespace Scald;

namespace
{
    const std::unordered_map<GBuffer::EGBufferLayer, DXGI_FORMAT> kGBufferFormats = {
        {GBuffer::EGBufferLayer::DIFFUSE_ALBEDO,    DXGI_FORMAT_R8G8B8A8_UNORM},
        {GBuffer::EGBufferLayer::AMBIENT_OCCLUSION, DXGI_FORMAT_R8G8B8A8_UNORM},
        {GBuffer::EGBufferLayer::NORMAL,            DXGI_FORMAT_R32G32B32A32_FLOAT},
        {GBuffer::EGBufferLayer::SPECULAR,          DXGI_FORMAT_R8G8B8A8_UNORM},
        {GBuffer::EGBufferLayer::MOTION_VECTORS,    DXGI_FORMAT_R16G16_FLOAT},
        {GBuffer::EGBufferLayer::DEPTH,             DXGI_FORMAT_D24_UNORM_S8_UINT}
    };

    constexpr FLOAT kDefaultOptimizedClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
}

GBuffer::GBuffer(Device* device, UINT width, UINT height)
    : m_device(device),
      m_width(width),
      m_height(height)
{
    CreateResources();
}

GBuffer::~GBuffer() noexcept {}

void GBuffer::OnResize(UINT newWidth, UINT newHeight)
{
    if (m_width != newWidth || m_height != newHeight)
    {
        m_width = newWidth;
        m_height = newHeight;

        CreateResources();

        // New resource, so we need new descriptors to that resource.
        CreateViews();
    }
}

ID3D12Resource* GBuffer::Get(const unsigned layer) const
{
    return m_textures[layer].Get();
}

DXGI_FORMAT GBuffer::GetBufferTextureFormat(const unsigned layer) const
{
    assert(layer >= 0 && layer < static_cast<unsigned>(EGBufferLayer::MAX) && "Invalid GBuffer layer has been specified");
    return kGBufferFormats.at(static_cast<EGBufferLayer>(layer));
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetGpuSrv(const unsigned layer) const
{
    assert(layer >= 0 && layer < static_cast<unsigned>(EGBufferLayer::MAX) && "Invalid GBuffer layer has been specified");
    return m_srvAllocation.GetGpuHandle(layer);
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetRtv(const unsigned layer) const
{
    assert(layer >= 0 && layer < static_cast<unsigned>(EGBufferLayer::MAX) && "Invalid GBuffer layer has been specified");
    assert(layer != EGBufferLayer::DEPTH && "You're trying to get a RTV for the depth layer, use GetDsv(layer) instead");
    return m_rtvAllocation.GetCpuHandle(layer);
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetDsv() const
{
    return m_dsvAllocation.GetCpuHandle();
}

void GBuffer::CreateDescriptors()
{
    // RTVs for every GBuffer layer except depth
    m_rtvAllocation = m_device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, static_cast<uint32_t>(EGBufferLayer::MAX) - 1u);
    // DSV for depth texture
    m_dsvAllocation = m_device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u);
    // SRVs for every GBuffer layer to bind to shader program
    m_srvAllocation = m_device->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, static_cast<uint32_t>(EGBufferLayer::MAX));

    CreateViews();
}

void GBuffer::CreateViews()
{
    // Create SRV to resource so we can sample the GBuffer texture in a shader program.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1u;

    // Create SRVs and RTVs for texture of GBuffer that aren't depth
    for (UINT i = 0; i < static_cast<UINT>(EGBufferLayer::DEPTH); i++)
    {
        srvDesc.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(i));
        m_device->GetD3D12Device()->CreateShaderResourceView(m_textures[i].Get(), &srvDesc, m_srvAllocation.GetCpuHandle(i));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        ZeroMemory(&rtvDesc, sizeof(rtvDesc));
        rtvDesc.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(i));
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0u;
        rtvDesc.Texture2D.PlaneSlice = 0u;
        m_device->GetD3D12Device()->CreateRenderTargetView(m_textures[i].Get(), &rtvDesc, m_rtvAllocation.GetCpuHandle(i));
    }

    // Create SRV for depth texture
    auto depthIndex = static_cast<UINT>(EGBufferLayer::DEPTH);
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    m_device->GetD3D12Device()->CreateShaderResourceView(m_textures[depthIndex].Get(), &srvDesc, m_srvAllocation.GetCpuHandle(depthIndex));

    // Create DSV for depth texture
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(depthIndex));
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0u;
    m_device->GetD3D12Device()->CreateDepthStencilView(m_textures[depthIndex].Get(), &dsvDesc, m_dsvAllocation.GetCpuHandle());
}

void GBuffer::CreateResources()
{
    CD3DX12_RESOURCE_DESC texDesc = {};
    ZeroMemory(&texDesc, sizeof(CD3DX12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = (UINT64)0;
    texDesc.Width = (UINT64)m_width;
    texDesc.Height = m_height;
    texDesc.DepthOrArraySize = (UINT16)1;
    texDesc.MipLevels = (UINT16)0;
    texDesc.SampleDesc.Count = 1u;
    texDesc.SampleDesc.Quality = 0u;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_CLEAR_VALUE optClear = {};
    ZeroMemory(&optClear, sizeof(D3D12_CLEAR_VALUE));

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    // Create resources for GBuffer that aren't depth
    for (UINT i = 0; i < static_cast<UINT>(EGBufferLayer::DEPTH); i++)
    {
        texDesc.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(i));
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        optClear.Format = texDesc.Format;

        if (i == EGBufferLayer::DIFFUSE_ALBEDO)  // To clear background at desirable color
            memcpy(optClear.Color, Colors::LightSteelBlue, sizeof(optClear.Color));
        else if (i == EGBufferLayer::MOTION_VECTORS)
            memcpy(optClear.Color, Colors::Yellow, sizeof(optClear.Color));
        else  // To clear to zero
            memcpy(optClear.Color, kDefaultOptimizedClearColor, sizeof(optClear.Color));

        ThrowIfFailed(m_device->GetD3D12Device()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_textures[i])));
    }

    auto depthIndex = static_cast<UINT>(EGBufferLayer::DEPTH);
    // Create resource for depth
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    texDesc.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(depthIndex));

    optClear.Format = kGBufferFormats.at(static_cast<EGBufferLayer>(depthIndex));
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = (UINT8)0;

    ThrowIfFailed(m_device->GetD3D12Device()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_textures[depthIndex])));

    SCALD_NAME_D3D12_OBJECT(m_textures[DIFFUSE_ALBEDO], L"Diffuse Buffer");
    SCALD_NAME_D3D12_OBJECT(m_textures[AMBIENT_OCCLUSION], L"WorldPos Buffer");  // SSAO will be
    SCALD_NAME_D3D12_OBJECT(m_textures[NORMAL], L"Normal Buffer");
    SCALD_NAME_D3D12_OBJECT(m_textures[SPECULAR], L"Specular Buffer");
    SCALD_NAME_D3D12_OBJECT(m_textures[MOTION_VECTORS], L"Motion Vectors Buffer");
    SCALD_NAME_D3D12_OBJECT(m_textures[DEPTH], L"Depth Buffer");
}