#pragma once

#include "DXHelper.h"

class RootSignature
{
public:
    RootSignature(D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1);

    /*RootSignature(const RootSignature& lhs) = delete;
    RootSignature& operator=(const RootSignature& lhs) = delete;
    RootSignature(RootSignature&& rhs) = delete;
    RootSignature& operator=(RootSignature&& rhs) = delete;*/

public:
    template <typename T = CD3DX12_STATIC_SAMPLER_DESC>
    void Create(ID3D12Device* device, UINT numParameters, const CD3DX12_ROOT_PARAMETER* rootParams,
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, UINT numStaticSamplers = 0u, const T* staticSamplers = nullptr)
    {
        if (numStaticSamplers)
        {
            // Root signature is an array of root parameters
            m_rootSignatureDesc.Init(numParameters, rootParams, numStaticSamplers, staticSamplers, flags);
        }
        else
        {
            auto staticSamplers = GetStaticSamplers();
            // Root signature is an array of root parameters
            m_rootSignatureDesc.Init(numParameters, rootParams, (UINT)staticSamplers.size(), staticSamplers.data(), flags);
        }

        ComPtr<ID3DBlob> signature = nullptr;
        ComPtr<ID3DBlob> error = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&m_rootSignatureDesc, m_rootSignatureVersion, &signature, &error));
        ThrowIfFailed(device->CreateRootSignature(0u, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    FORCEINLINE ID3D12RootSignature* Get() const { return m_rootSignature.Get(); }

    FORCEINLINE const CD3DX12_ROOT_SIGNATURE_DESC& GetDesc() const { return m_rootSignatureDesc; }

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 5> GetStaticSamplers();

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    CD3DX12_ROOT_SIGNATURE_DESC m_rootSignatureDesc;
    D3D_ROOT_SIGNATURE_VERSION m_rootSignatureVersion;
};