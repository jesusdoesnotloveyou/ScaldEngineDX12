#pragma once

#ifndef GBufferSize
#define GBufferSize 5 // DiffuseAlbedo, LightAccumulation, Normal, Depth, Specular
#endif

#include "Common/DXHelper.h"

class GBuffer final
{
public:

	GBuffer();
	GBuffer(const GBuffer& buffer) = delete;
	GBuffer& operator=(const GBuffer& buffer) = delete;

	~GBuffer() noexcept;

private:
	void CreateResources();


private:

	ComPtr<ID3D12Resource> Buffer[GBufferSize];
};