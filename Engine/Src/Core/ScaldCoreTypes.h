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
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT2 texCoord = XMFLOAT2(0.0f, 0.0f);

	SVertex() {}

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
	XMFLOAT4X4 InvViewProj;

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float pad1 = 0.0f;
	
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;
};

struct MaterialConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
};