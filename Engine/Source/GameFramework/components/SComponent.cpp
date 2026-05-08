#include "stdafx.h"
#include "SComponent.h"

Scald::SComponent::~SComponent() noexcept {}

void Scald::SComponent::Initialize() {}

void Scald::SComponent::Update() {}

void Scald::SComponent::OnAttach() {}

void Scald::SComponent::OnDestroy() {}

std::shared_ptr<Scald::SEntity> Scald::SComponent::GetOwner() const
{
    return m_owner.lock();
}

void Scald::SComponent::SetOwner(std::shared_ptr<SEntity>& entity)
{
    m_owner = entity;
}