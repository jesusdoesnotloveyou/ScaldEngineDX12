#pragma once

#include "Common/DXHelper.h"
#include "Common/ScaldTimer.h"

class ScaldComponent;

namespace Scald
{
	class Object : std::enable_shared_from_this<Object>
	{
	public:
		Object() = default;
		virtual ~Object() noexcept;
	
		virtual void OnInit() {};
		virtual void OnUpdate();
		virtual void OnBegin() {};
		virtual void OnDestroy() {};

	protected:
		std::vector<std::shared_ptr<ScaldComponent>> m_components;

	};
}