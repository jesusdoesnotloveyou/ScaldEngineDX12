#pragma once

#include "Common/DXHelper.h"

class RootSignature
{
public:
	RootSignature() = default;

	RootSignature(const RootSignature& lhs) = delete;
	RootSignature& operator=(const RootSignature& lhs) = delete;
	RootSignature(RootSignature&& rhs) = delete;
	RootSignature& operator=(RootSignature&& rhs) = delete;

public:

};