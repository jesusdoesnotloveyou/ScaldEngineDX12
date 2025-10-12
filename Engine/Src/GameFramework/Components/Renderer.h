#pragma once

#include "SComponent.h"

namespace Scald
{
	class Renderer : public SComponent
	{
		using Super = SComponent;
	public:
		Renderer(std::shared_ptr<SObject> owner);
		virtual ~Renderer() noexcept override;

		// For frustum culling
		BoundingBox Bounds;

		XMMATRIX World = XMMatrixIdentity();
		// could be used for texture tiling
		XMMATRIX TexTransform = XMMatrixIdentity();
	};
}