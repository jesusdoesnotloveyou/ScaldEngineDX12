#pragma once

#include "Object.h"

namespace Scald
{
	class Actor : public Object
	{
		using Super = Object;

	public:
		Actor() = default;
		virtual ~Actor() noexcept override;

		//~ Begin of Object interface
		virtual void OnInit() override {};
		virtual void OnUpdate() override {};
		virtual void OnBegin() override {};
		virtual void OnDestroy() override {};
		//~ End of Object interface
	};
}