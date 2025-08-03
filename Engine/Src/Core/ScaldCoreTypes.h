#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

// Constant buffers

struct ObjectConstants
{
	XMFLOAT4X4 gWorldViewProj;
	float pad[48];
};
static_assert((sizeof(ObjectConstants) % 256) == 0, "Constant Buffer size must be 256-byte aligned");