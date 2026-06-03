#pragma once

#include <memory>

namespace Scald
{
class SComponent;
class SEntity;

class ComponentManager
{
public:
    static ComponentManager& Get()
    {
        static ComponentManager inst;
        return inst;
    }

    template <typename T = SComponent, typename... Args>
    std::shared_ptr<T> CreateDefaultSubobject(std::shared_ptr<SEntity> owner, Args&&... args)
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
}  // namespace Scald