#pragma once

#include "SObject.h"

namespace Scald
{
	class Actor : public SObject
	{
		using Super = SObject;

	public:
		Actor() = default;
		virtual ~Actor() noexcept override;

		//~ Begin of SObject interface
		virtual void OnInit() override {};
		virtual void OnUpdate() override {};
		virtual void OnBegin() override {};
		virtual void OnDestroy() override {};
		//~ End of SObject interface
	};
}