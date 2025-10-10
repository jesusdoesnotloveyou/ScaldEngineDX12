#pragma once

#ifndef GBufferSize
#define GBufferSize 5 // DiffuseAlbedo, LightAccumulation, Normal, Depth, Specular
#endif

#include "Common/DXHelper.h"

class GBuffer final
{
public:
	GBuffer(ID3D12Device* device);
	GBuffer(const GBuffer& buffer) = delete;
	GBuffer& operator=(const GBuffer& buffer) = delete;

	~GBuffer() noexcept;

private:
	void CreateResources();


private:
	ID3D12Device* m_device = nullptr;
	ComPtr<ID3D12Resource> m_buffer[GBufferSize];
	DXGI_FORMAT m_bufferFormats[GBufferSize];
};