#include "stdafx.h"
#include "Camera.h"

Camera::Camera()
{
	m_view = XMMatrixIdentity();
	m_proj = XMMatrixIdentity();
}

XMMATRIX Camera::GetViewMatrix() const
{
	return XMMATRIX{};
}

XMMATRIX Camera::GetPerspectiveProjectionMatrix() const
{
	return XMMATRIX{};
}

XMMATRIX Camera::GetOrthoProjectionMatrix() const
{
	return XMMATRIX{};
}