#pragma once

#include "DXHelper.h"

using Microsoft::WRL::ComPtr;

class ScaldUtil
{
public:
    static ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer);

    static UINT CalcConstantBufferByteSize(const UINT byteSize);
};