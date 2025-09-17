#pragma once

#include "ScaldCoreTypes.h"
#include "DXHelper.h"
#include <vector>

class Shapes
{
public:
	struct MeshData
	{
		std::vector<SVertex> vertices;
		std::vector<uint16_t> indices;
	};

	static MeshData CreateBox(float width, float height, float depth);

	static MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
	static MeshData CreateGrid(float width, float depth, UINT m, UINT n);
};