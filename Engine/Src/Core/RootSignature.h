#pragma once

#include "Common/DXHelper.h"

class RootSignature
{
public:
	RootSignature();

	RootSignature(const RootSignature& lhs) = delete;
	RootSignature& operator=(const RootSignature& lhs) = delete;
	RootSignature(RootSignature&& rhs) = delete;
	RootSignature& operator=(RootSignature&& rhs) = delete;

public:
	static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 5> GetStaticSamplers();
};