#include "SEntity.h"
#include "GameFramework/components/SComponent.h"

using namespace Scald;

SEntity::SEntity(std::string&& entityName)
    : m_name(std::move(entityName))
{
}

void SEntity::Initialize()
{
    for (auto& comp : m_components)
    {
        comp->Initialize();
    }
}

void SEntity::Update()
{
    if (!mIsActive) return;

    for (auto& comp : m_components)
    {
        comp->Update();
    }
}