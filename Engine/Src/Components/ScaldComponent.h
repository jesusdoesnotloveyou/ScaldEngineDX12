#pragma once

#include "Common/DXHelper.h"

namespace Scald
{
	class ScaldComponent : std::enable_shared_from_this<ScaldComponent>
	{
		ScaldComponent() = default;
		virtual ~ScaldComponent() noexcept;
	};
}