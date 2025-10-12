#pragma once

#include "Common/DXHelper.h"

namespace Scald
{
	class SObject;

	class SComponent : public std::enable_shared_from_this<SComponent>
	{
		friend class SObject;
		// to prevent manual creation
	protected:
		SComponent(std::shared_ptr<SObject> owner)
		{
			m_owner = owner;
		}
		
	public:
		virtual ~SComponent() noexcept;

		virtual void OnUpdate();
		virtual void OnAttach();
		virtual void OnDestroy();

		std::shared_ptr<SObject> GetOwner() const
		{
			return m_owner.lock();
		}
	
	protected:
		std::weak_ptr<SObject> m_owner;
	};
}