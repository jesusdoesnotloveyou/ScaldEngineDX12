#pragma once

#include "DXHelper.h"

class Camera
{
public:
	Camera();

	virtual ~Camera() = default;

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetPerspectiveProjectionMatrix() const;
	XMMATRIX GetOrthoProjectionMatrix() const;

private:
	XMMATRIX m_view = XMMatrixIdentity();
	XMMATRIX m_proj = XMMatrixIdentity();
};