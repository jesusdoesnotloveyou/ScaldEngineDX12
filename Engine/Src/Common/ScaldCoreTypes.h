#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

#ifndef FORCEINLINE
	#define FORCEINLINE __forceinline
#endif

using namespace DirectX;

enum class EPassType : uint8_t
{
	DepthShadow = 0,
	DeferredGeometry,
	DeferredColor,
	NumPasses = 3
};

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

#define MaxCascades 4

struct CascadesShadows
{
	CascadesShadows()
	{
		for (int i = 0; i < MaxCascades; ++i)
		{
			CascadeViewProj[i] = XMMatrixIdentity();
			Distances[i] = 0.0f;
		}
	}

	XMMATRIX CascadeViewProj[MaxCascades];
	float Distances[MaxCascades];
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
	XMFLOAT4X4 InvTransposeWorld;
	XMFLOAT4X4 TexTransform;
	UINT MaterialIndex = 0u;
	UINT objPad0 = 0u;
	UINT objPad1 = 0u;
	UINT objPad2 = 0u;
};

#define MaxLights 16

struct PassConstants
{
	XMFLOAT4X4 View;
	XMFLOAT4X4 Proj;
	XMFLOAT4X4 ViewProj;
	XMFLOAT4X4 InvViewProj;

	CascadesShadows Cascades;

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float pad1 = 0.0f;
	
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;

	XMFLOAT4 Ambient = { 0.0f, 0.0f, 0.0f, 1.0f };

	XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float FogStart = 8.0f;
	float FogRange = 18.0f;
	XMFLOAT2 pad2 = { 0.0f, 0.0f };

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];
};

// Structured buffers
struct MaterialData
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	XMFLOAT4X4 MatTransform;
	UINT DiffusseMapIndex = 0u;
	UINT matPad0 = 0u;
	UINT matPad1 = 0u;
	UINT matPad2 = 0u;
};