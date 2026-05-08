#pragma once

#include "Common/DXHelper.h"

namespace Scald
{
class SEntity;

class SComponent : public std::enable_shared_from_this<SComponent>
{
    friend class SEntity;
    // to prevent manual creation
protected:
    SComponent(std::shared_ptr<SEntity> owner) { m_owner = owner; }

public:
    virtual ~SComponent() noexcept;

    virtual void Initialize();
    virtual void Update();
    virtual void OnAttach();
    virtual void OnDestroy();

    std::shared_ptr<SEntity> GetOwner() const;
    void SetOwner(std::shared_ptr<SEntity>& owner);

protected:
    std::weak_ptr<SEntity> m_owner;
};
}  // namespace Scald