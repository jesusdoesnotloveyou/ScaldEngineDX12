#pragma once

#include "Common/ScaldCoreTypes.h"
#include "Common/DXHelper.h"

class Shapes
{
public:
	static MeshData CreateBox(float width, float height, float depth);

	static MeshData CreateSphere(float radius, UINT sliceCount, UINT stackCount);

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
	static MeshData CreateGrid(float width, float depth, UINT m, UINT n);
};