#include "Transform.h"

Scald::Transform::Transform(std::shared_ptr<Scald::SEntity> owner, XMVECTOR pos, XMVECTOR rot, XMVECTOR scale)
    : Super(owner)
{
    XMStoreFloat3(&m_translation, pos);
    XMStoreFloat3(&m_euler, rot);
    XMStoreFloat3(&m_scale, scale);
    XMStoreFloat4(&m_orient, XMVectorZero());
}

Scald::Transform::~Transform() noexcept {}

void Scald::Transform::Update()
{
    if (!m_bIsDirty) return;

    // m_world = XMMatrixScaling();

    m_bIsDirty = false;
}

void Scald::Transform::OnAttach() {}

void Scald::Transform::OnDestroy() {}
