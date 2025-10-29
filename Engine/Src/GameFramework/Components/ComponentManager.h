#pragma once

#include "Common/DXHelper.h"

namespace Scald
{
	class SComponent;
	class SObject;
	
	class ComponentManager
	{
	public:
		static ComponentManager& Get()
		{
			static ComponentManager inst;
			return inst;
		}

		template<typename T = SComponent, typename... Args>
		std::shared_ptr<T> CreateDefaultSubobject(std::shared_ptr<SObject> owner, Args&&... args)
		{
			return std::make_shared<T>(owner, std::forward<Args>(args)...);
		}

	private:
		ComponentManager() = default;
		~ComponentManager() = default;

		ComponentManager(const ComponentManager&) = delete;
		ComponentManager& operator=(const ComponentManager&) = delete;
		ComponentManager(ComponentManager&&) = delete;
		ComponentManager& operator=(ComponentManager&&) = delete;
	};
}