#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct VertexPosition
{
	VertexPosition() {}

	VertexPosition(const XMFLOAT3& p)
		: position(p)
	{}

	VertexPosition(float px, float py, float pz)
		: position(px, py, pz)
	{}

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);

private:
	static constexpr inline UINT InputElementCount = 1u;
	static constexpr inline const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount] =
	{
		{ "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
	};

public:
	static constexpr inline D3D12_INPUT_LAYOUT_DESC InputLayout =
	{
		InputElements,
		InputElementCount
	};
};

struct VertexPositionNormalTangentUV
{
	VertexPositionNormalTangentUV() {}

	VertexPositionNormalTangentUV(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT3& t, const XMFLOAT2& uv)
		: position(p)
		, normal(n)
		, tangent(t)
		, texCoord(uv)
	{
	}

	VertexPositionNormalTangentUV(
		float px, float py, float pz,
		float nx, float ny, float nz,
		float tx, float ty, float tz,
		float u, float v)
		: position(px, py, pz)
		, normal(nx, ny, nz)
		, tangent(tx, ty, tz)
		, texCoord(u, v)
	{
	}

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT2 texCoord = XMFLOAT2(0.0f, 0.0f);
private:
	static constexpr inline UINT InputElementCount = 4u;
	static constexpr inline const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount] =
	{
		{ "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
		{ "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 12u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
		{ "TANGENT", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 24u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
		{ "TEXCOORD", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 36u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u },
	};

public:
	static constexpr inline D3D12_INPUT_LAYOUT_DESC InputLayout =
	{
		InputElements,
		InputElementCount
	};
};