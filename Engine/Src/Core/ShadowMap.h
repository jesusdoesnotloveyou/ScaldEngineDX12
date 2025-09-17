#pragma once

#include "DXHelper.h"

using namespace Microsoft::WRL;

class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device, UINT width, UINT height);
	ShadowMap(const ShadowMap& lhs) = delete;
	ShadowMap& operator=(const ShadowMap& lhs) = delete;

	virtual ~ShadowMap() noexcept;

public:
	UINT GetWidth()const;
	UINT GetHeight()const;

	ID3D12Resource* Get();

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv()const;
	
	D3D12_VIEWPORT GetViewport()const;
	D3D12_RECT GetScissorRect()const;

	void CreateDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void CreateDescriptors();
	void CreateResource();
private:
	ID3D12Device* m_device = nullptr;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsv;

	DXGI_FORMAT m_format = DXGI_FORMAT_R24G8_TYPELESS;

	UINT m_mapWidth = 0u;
	UINT m_mapHeight = 0u;

	// actual gpu resource
	ComPtr<ID3D12Resource> m_shadowMap = nullptr;
};