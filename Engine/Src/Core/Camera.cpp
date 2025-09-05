#include "stdafx.h"
#include "Camera.h"
#include "ScaldMath.h"

Camera::Camera()
	:
	m_radius(5.0f),
	m_fovAngleY(0.0f),
	m_nearZ(1.0f),
	m_farZ(1000.0f),
	m_phi(XM_PIDIV4),
	m_theta(1.5f * XM_PI)
{
	m_view = XMMatrixIdentity();
	m_proj = XMMatrixIdentity();
}

void Camera::Update(float deltaTime)
{
	// Convert Spherical to Cartesian
	float x = m_radius * sinf(m_phi) * cosf(m_theta);
	float z = m_radius * sinf(m_phi) * sinf(m_theta);
	float y = m_radius * cosf(m_phi);

	// View matrix
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_view = XMMatrixLookAtLH(pos, target, up);
}

void Camera::Reset(float fovAngleY, float aspectRatio, float nearZ, float farZ)
{
	m_proj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearZ, farZ);
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