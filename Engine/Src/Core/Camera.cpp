#include "stdafx.h"
#include "Camera.h"
#include "Common/ScaldMath.h"

Camera::Camera()
	: m_nearZ(1.0f)
	, m_farZ(1000.0f)
	, m_fovYRad(0.0f)
	, m_aspectRatio(0.0f)
	, m_nearWindowHeight(0.0f)
	, m_farWindowHeight(0.0f)
{
	m_view = XMMatrixIdentity();
	m_persProj = XMMatrixIdentity();
	m_orthProj = XMMatrixIdentity();
	// to update view matrix on the first update call
	m_isDirty = true;
}

void Camera::Update(float deltaTime)
{
	if (!m_isDirty) return;

	// View matrix
	XMVECTOR pos = GetPosition();
	XMVECTOR target = pos + XMLoadFloat3(&m_forward);
	XMVECTOR up = XMLoadFloat3(&m_up);

	m_view = XMMatrixLookAtLH(pos, target, up);

	m_isDirty = false;
}

void Camera::Reset(float fovAngleYDegrees, float aspectRatio, float nearZ, float farZ)
{
	m_nearZ = nearZ;
	m_farZ = farZ;
	m_fovYRad = (fovAngleYDegrees / 180.0f) * XM_PI;
	m_aspectRatio = aspectRatio;

	m_nearWindowHeight = 2.0f * tanf(0.5f * m_fovYRad) * m_nearZ;
	m_farWindowHeight = 2.0f * tanf(0.5f * m_fovYRad) * m_farZ;

	m_persProj = XMMatrixPerspectiveFovLH(m_fovYRad, m_aspectRatio, m_nearZ, m_farZ);
	m_orthProj = XMMatrixOrthographicLH(GetNearWindowWidth(), GetNearWindowHeight(), nearZ, farZ);

	BoundingFrustum::CreateFromMatrix(m_frustum, m_persProj);
}

XMMATRIX Camera::GetViewMatrix() const
{
	return m_view;
}

XMMATRIX Camera::GetPerspectiveProjectionMatrix() const
{
	return m_persProj;
}

XMMATRIX Camera::GetOrthoProjectionMatrix() const
{
	return m_orthProj;
}

float Camera::GetFovYRad() const
{
	return m_fovYRad;
}

float Camera::GetFovXRad() const
{
	return 2.0f * atanf(GetFarWindowWidth() * 0.5f / m_farZ);
}

XMFLOAT3 Camera::GetPosition3f() const
{
	return m_position;
}

XMVECTOR Camera::GetPosition() const
{
	return XMLoadFloat3(&m_position);
}

void Camera::SetPosition(float x, float y, float z)
{
	m_position = XMFLOAT3(x, y, z);
	m_isDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
	m_position = v;
	m_isDirty = true;
}

void Camera::MoveRight(float d)
{
	// m_position += d * m_right;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&m_right);
	XMVECTOR p = GetPosition();
	XMStoreFloat3(&m_position, XMVectorMultiplyAdd(s, r, p));
	m_isDirty = true;
}

void Camera::MoveForward(float d)
{
	// m_position += d * m_forward;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR f = XMLoadFloat3(&m_forward);
	XMVECTOR p = GetPosition();
	XMStoreFloat3(&m_position, XMVectorMultiplyAdd(s, f, p));
	m_isDirty = true;
}

// Rotation around world's Y axis
void Camera::AdjustYaw(float angle)
{
	XMMATRIX R = XMMatrixRotationY(angle);
	XMStoreFloat3(&m_right,	  XMVector3TransformNormal(XMLoadFloat3(&m_right), R));
	XMStoreFloat3(&m_up,	  XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
	XMStoreFloat3(&m_forward, XMVector3TransformNormal(XMLoadFloat3(&m_forward), R));
	m_isDirty = true;
}

// Rotation around X axis
void Camera::AdjustPitch(float angle)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle);
	XMStoreFloat3(&m_up,	  XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
	XMStoreFloat3(&m_forward, XMVector3TransformNormal(XMLoadFloat3(&m_forward), R));
	m_isDirty = true;
}