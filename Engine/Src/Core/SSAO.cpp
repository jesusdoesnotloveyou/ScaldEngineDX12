#include "stdafx.h"
#include "SSAO.h"

SSAO::SSAO(ID3D12Device* device, UINT width, UINT height)
	: m_device(device)
	, m_renderTargetWidth(width)
	, m_renderTargetHeight(height)
{
	OnResize(width, height);


}

void SSAO::OnResize(UINT newWidth, UINT newHeight)
{
	if (m_renderTargetWidth != newWidth || m_renderTargetHeight != newHeight)
	{
		m_renderTargetWidth = newWidth;
		m_renderTargetHeight = newHeight;

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = m_renderTargetWidth / 2.0f;
		m_viewport.Height = m_renderTargetHeight / 2.0f;
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		m_scissorRect = { 0L, 0L, static_cast<LONG>(m_renderTargetWidth / 2), static_cast<LONG>(m_renderTargetHeight / 2) };

		BuildResources();
	}
}

void SSAO::RebuildDescriptors()
{
	
}

void SSAO::BuildResources()
{

}