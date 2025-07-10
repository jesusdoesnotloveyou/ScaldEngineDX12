#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

// Constant buffers

struct SceneConstantBuffer
{
	XMFLOAT4 offset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	float pad[60];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");