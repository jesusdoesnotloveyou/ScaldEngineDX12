#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

struct SVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT2 texCoord;

	SVertex(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT3& t, const XMFLOAT2& uv)
		:
		position(p),
		normal(n),
		tangent(t),
		texCoord(uv)
	{}
	
	SVertex(
		float px, float py, float pz,
		float nx, float ny, float nz,
		float tx, float ty, float tz,
		float u, float v)
		:
		position(px, py, pz),
		normal(nx, ny, nz),
		tangent(tx, ty, tz),
		texCoord(u, v)
	{}
};

// Constant buffers
struct ObjectConstants
{
	XMFLOAT4X4 World;
};

struct PassConstants
{
	XMFLOAT4X4 View;
	XMFLOAT4X4 Proj;
	XMFLOAT4X4 ViewProj;
	XMFLOAT4X4 invViewProj;

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float pad1 = 0.0f;
	
	float NearZ;
	float FarZ;
	float DeltaTime;
	float TotalTime;
};