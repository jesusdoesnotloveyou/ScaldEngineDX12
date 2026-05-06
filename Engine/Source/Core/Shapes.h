#pragma once

#include "Common/ScaldCoreTypes.h"
#include "Common/DXHelper.h"

class Shapes
{
public:
	static MeshData<VertexPositionNormalTangentUV, uint16_t> CreateBox(float width, float height, float depth);

	static MeshData<VertexPositionNormalTangentUV, uint16_t> CreateSphere(float radius, UINT sliceCount, UINT stackCount);

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
	static MeshData<VertexPositionNormalTangentUV, uint16_t> CreateGrid(float width, float depth, UINT m, UINT n);

	static MeshData<VertexPositionNormalTangentUV, uint16_t> CreateGeosphere(float radius, UINT numSubdivisions);

private:
	static void Subdivide(MeshData<>& meshData);
	static VertexPositionNormalTangentUV MidPoint(const VertexPositionNormalTangentUV& v0, const VertexPositionNormalTangentUV& v1);
};