#pragma once

#include "Common/DXHelper.h"

class SSAO
{

public:
	SSAO(ID3D12Device* device, UINT width, UINT height);
	SSAO(const SSAO& ssao) = delete;
	SSAO& operator=(const SSAO& ssao) = delete;

	~SSAO() noexcept = default;

public:
	void OnResize(UINT newWidth, UINT newHeight);

	D3D12_VIEWPORT GetViewport() const { return m_viewport; }
	D3D12_RECT GetScissorRect() const { return m_scissorRect; }

private:
	void RebuildDescriptors();
	void BuildResources();

private:
	ID3D12Device* m_device = nullptr;

	UINT m_renderTargetWidth;
	UINT m_renderTargetHeight;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
};