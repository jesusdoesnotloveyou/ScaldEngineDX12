#pragma once

#include "Common/DXHelper.h"

enum EGBufferLayer : uint8_t
{
	DIFFUSE_ALBEDO = 0,
	LIGHT_ACCUM,
	NORMAL,
	SPECULAR,
	DEPTH,	

	MAX = 5
};

struct FGBufferTexture
{
	ComPtr<ID3D12Resource> m_resource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuRtvDsv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv = {};

	bool m_isDSV = false;
};

class GBuffer final
{
public:
	GBuffer(ID3D12Device* device, UINT width, UINT height);
	GBuffer(const GBuffer& buffer) = delete;
	GBuffer& operator=(const GBuffer& buffer) = delete;

	~GBuffer() noexcept;

public:
	// if screen resized
	void OnResize(UINT newWidth, UINT newHeight);

	FGBufferTexture* GetBufferTexture(EGBufferLayer bufferLayer)
	{
		return &m_buffer[bufferLayer];
	}

	DXGI_FORMAT GetBufferTextureFormat(EGBufferLayer bufferLayer)
	{
		return m_bufferFormats[bufferLayer];
	}

private:
	void CreateResources();
	void CreateDescriptors();

private:
	ID3D12Device* m_device = nullptr;

	UINT m_width, m_height;

	FGBufferTexture m_buffer[EGBufferLayer::MAX];
	// maybe better decision to create a map [EGBufferLayer, DXGI_FORMAT]
	static constexpr DXGI_FORMAT m_bufferFormats[static_cast<int>(EGBufferLayer::MAX)] = // order of DXGI_FORMAT should corresponds to EGBufferLayer 
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,			//DIFFUSE_ALBEDO
		DXGI_FORMAT_R8G8B8A8_UNORM,			//LIGHT_ACCUM
		DXGI_FORMAT_R32G32B32A32_FLOAT,		//NORMAL
		DXGI_FORMAT_R8G8B8A8_UNORM,			//SPECULAR
		DXGI_FORMAT_D24_UNORM_S8_UINT		//DEPTH
	};
};