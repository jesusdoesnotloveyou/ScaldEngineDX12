#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texC;
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

struct Light
{
	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FallOfStart = 1.0f;					// spot/point
	XMFLOAT3 Direction = { 0.5f, -1.0f, 0.5f }; // spot/dir
	float FallOfEnd = 10.0f;					// spot/point
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };	// spot/point
	float SpotPower = 64.0f;					// spot only
};

// Constant buffers
struct ObjectConstants
{
	XMFLOAT4X4 World;
	XMFLOAT4X4 TexTransform;
};

#define MaxLights 16

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

	XMFLOAT4 Ambient = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];
};

struct MaterialConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	XMFLOAT4X4 MatTransform;
};