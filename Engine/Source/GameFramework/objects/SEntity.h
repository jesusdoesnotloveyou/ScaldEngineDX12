#pragma once

#include "GameFrameWork/Components/ComponentManager.h"

namespace Scald
{
class SComponent;

class SCALD_API SEntity : public std::enable_shared_from_this<SEntity>
{
public:
    explicit SEntity(std::string& entityName);
    virtual /*for polymorphic call when object is destructed*/ ~SEntity() noexcept = default;

    virtual void Initialize();
    virtual void Update();
    virtual void OnBegin() {};
    virtual void OnDestroy() {};

    bool IsActive() const { return mIsActive; }
    void SetActive(bool isActive) { mIsActive = isActive; }

    const std::string GetName() const { return m_name; }

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

    template <typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<SComponent, T>::value, "T must be derived from Component");

        auto comp = ComponentManager::Get().CreateDefaultSubobject<T>(shared_from_this(), std::forward<Args>(args)...);
        comp->OnAttach();
        m_components.push_back(comp);
        return comp.get();
    }

    template <typename T>
    bool RemoveComponent()
    {
        static_assert(std::is_base_of<SComponent, T>::value, "T must be derived from Component");

        for (auto it = m_components.begin(); it != m_components.end(); it++)
        {
            if (std::dynamic_pointer_cast<T>(*it))
            {
                m_components.erase(it);
                return true;
            }
        }
        return false;
    }

protected:
    std::string m_name;
    std::vector<std::shared_ptr<SComponent>> m_components;  // in Vulkan doc about engine architecture components are stored as unique ptrs

    bool mIsActive = true;
};
}  // namespace Scald