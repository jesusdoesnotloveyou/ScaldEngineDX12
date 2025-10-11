#pragma once

#include "Common/DXHelper.h"
#include "Common/ScaldTimer.h"
#include "Components/ComponentManager.h"

namespace Scald
{
    class SComponent;

    class SObject : public std::enable_shared_from_this<SObject>
	{
	public:
		SObject() = default;
		virtual ~SObject() noexcept = default;
	
		virtual void OnInit() {};
        virtual void OnUpdate() {};
		virtual void OnBegin() {};
		virtual void OnDestroy() {};

        template <typename T>
        const std::vector<std::shared_ptr<T>>& GetComponents() const
        {
            return m_components;
        }

        template <typename T>
        std::vector<std::shared_ptr<T>> GetComponents() const
        {
            std::vector<std::shared_ptr<T>> foundComps;

            for (auto&& comp : m_components)
            {
                if (std::shared_ptr<T> castedComp = std::dynamic_pointer_cast<T>(comp))
                {
                    foundComps.push_back(castedComp);
                }
            }
            return foundComps;
        }

        template <typename T>
        std::shared_ptr<T> GetComponent() const
        {
            for (auto&& comp : m_components)
            {
                if (std::shared_ptr<T> castedComp = std::dynamic_pointer_cast<T>(comp))
                {
                    return castedComp;
                }
            }
            return nullptr;
        }

        template<typename T, typename... Args>
        void AddComponent(Args&&... args)
        {
            auto comp = ComponentManager::Get().CreateDefaultSubobject<T>(shared_from_this(), std::forward<Args>(args)...);
            comp->OnAttach();
            m_components.push_back(comp);
        }

	protected:
		std::vector<std::shared_ptr<SComponent>> m_components;
	};
}