#include "stdafx.h"
#include "Camera.h"
#include "Common/ScaldMath.h"

Camera::Camera()
	: m_radius(5.0f)
	, m_nearZ(1.0f)
	, m_farZ(1000.0f)
	, m_phi(XM_PIDIV4)
	, m_theta(1.5f * XM_PI)
	, m_x(0.0f)
	, m_y(0.0f)
	, m_z(0.0f)
{
	m_view = XMMatrixIdentity();
	m_proj = XMMatrixIdentity();
}

void Camera::Update(float deltaTime)
{
	// Convert Spherical to Cartesian
	m_x = m_radius * sinf(m_phi) * cosf(m_theta);
	m_z = m_radius * sinf(m_phi) * sinf(m_theta);
	m_y = m_radius * cosf(m_phi);

	// View matrix
	XMVECTOR pos = XMVectorSet(m_x, m_y, m_z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_view = XMMatrixLookAtLH(pos, target, up);
}

void Camera::Reset(float fovAngleY, float aspectRatio, float nearZ, float farZ)
{
	m_proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearZ, farZ);
	m_nearZ = nearZ;
	m_farZ = farZ;
}

XMMATRIX Camera::GetViewMatrix() const
{
	return m_view;
}

XMMATRIX Camera::GetPerspectiveProjectionMatrix() const
{
	return m_proj;
}

XMMATRIX Camera::GetOrthoProjectionMatrix() const
{
	return XMMATRIX{};
}

XMFLOAT3 Camera::GetPosition() const
{
	return XMFLOAT3(m_x, m_y, m_z);
}

void Camera::AdjustCameraRadius(float adjustValue)
{
	m_radius += adjustValue;
	m_radius = ScaldMath::Clamp(m_radius, 3.0f, 15.0f);
}

void Camera::AdjustYaw(float adjustYawValue)
{
	m_theta += adjustYawValue;
}

void Camera::AdjustPitch(float adjustPitchValue)
{
	m_phi += adjustPitchValue;
	m_phi = ScaldMath::Clamp(m_phi, 0.1f, XM_PI - 0.1f);
}