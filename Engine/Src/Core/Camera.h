#pragma once

#include "Common/DXHelper.h"

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

	XMFLOAT3 GetPosition() const;

	void AdjustCameraRadius(float adjustRadiusValue);
	void AdjustYaw(float adjustYawValue);
	void AdjustPitch(float adjustPitchValue);

private:
	float m_radius;
	float m_nearZ;
	float m_farZ;

	float m_phi;
	float m_theta;

	float m_x;
	float m_y;
	float m_z;

	XMMATRIX m_view = XMMatrixIdentity();
	XMMATRIX m_proj = XMMatrixIdentity();
};