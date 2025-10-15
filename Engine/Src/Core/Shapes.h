#pragma once

#include "Common/ScaldCoreTypes.h"
#include "Common/DXHelper.h"

class Shapes
{
public:
	static MeshData<SVertex, uint16_t> CreateBox(float width, float height, float depth);

	static MeshData<SVertex, uint16_t> CreateSphere(float radius, UINT sliceCount, UINT stackCount);

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
	static MeshData<SVertex, uint16_t> CreateGrid(float width, float depth, UINT m, UINT n);
};