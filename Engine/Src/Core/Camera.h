#pragma once

#include "DXHelper.h"

class Camera
{
public:
	Camera();
	/*virtual */~Camera() = default;

	void Update(float deltaTime);
	void Reset(float fovAngleY, float aspectRatio, float nearZ, float farZ);

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetPerspectiveProjectionMatrix() const;
	XMMATRIX GetOrthoProjectionMatrix() const;

	void AdjustCameraRadius(float adjustRadiusValue);
	void AdjustYaw(float adjustYawValue);
	void AdjustPitch(float adjustPitchValue);

private:
	float m_radius;
	float m_fovAngleY;
	float m_nearZ;
	float m_farZ;

	float m_phi;
	float m_theta;

	XMMATRIX m_view = XMMatrixIdentity();
	XMMATRIX m_proj = XMMatrixIdentity();
};