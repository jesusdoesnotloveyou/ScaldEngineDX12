#pragma once

#include "Common/DXHelper.h"
#include <cstdint>

enum class QueueType : uint8_t
{
	Direct,
	Copy,
	Compute,
	Count = 3
};

class SDevice : std::enable_shared_from_this<SDevice>
{
	/* I decided to place them in device class, since they're related to a specific system device (i.e. GPU) */
	
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<IDXGIAdapter1> m_hardwareAdapter;

	UINT m_dxgiFactoryFlags = 0u;

public:


};