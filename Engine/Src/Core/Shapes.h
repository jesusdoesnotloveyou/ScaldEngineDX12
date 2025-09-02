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
};