#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
	:
	 m_device(device)
	,m_mapWidth(width)
	,m_mapHeight(height)
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

	CreateResource();
}

ShadowMap::~ShadowMap() noexcept
{
}

UINT ShadowMap::GetWidth()const
{
	return m_mapWidth;
}
	
UINT ShadowMap::GetHeight()const
{
	return m_mapHeight;
}

ID3D12Resource* ShadowMap::Get()
{
	return m_shadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::GetSrv()const
{
	return m_hGpuSrv;
}
	
CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDsv()const
{
	return m_hCpuDsv;
}

D3D12_VIEWPORT ShadowMap::GetViewport()const
{
	return m_viewport;
}

D3D12_RECT ShadowMap::GetScissorRect()const
{
	return m_scissorRect;
}

void ShadowMap::CreateDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
}

void ShadowMap::CreateDescriptors()
{
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

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&m_shadowMap)));
}