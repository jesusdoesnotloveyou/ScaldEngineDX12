#include "stdafx.h"
#include "SEntity.h"
#include "GameFramework/components/SComponent.h"

Scald::SEntity::SEntity(std::string& entityName)
	:
	m_name(entityName)
{

}

void Scald::SEntity::Initialize()
{
	for (auto& comp : m_components)
	{
		comp->Initialize();
	}
}

void Scald::SEntity::Update()
{
	if (!mIsActive) return;

	for (auto& comp : m_components)
	{
		comp->Update();
	}
}