#pragma once
#pragma once

#include "Common/DXHelper.h"
#include <d3d11.h>

static constexpr int injectionBufferSize = 1024;

class ParticleSystem
{
public:
    ParticleSystem(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int maxParticles, XMVECTOR origin, class Camera* camera);
    virtual void Update(float elapsedTime) = 0;
    virtual void Simulate(float elapsedTime) = 0;
    virtual void Render();
    virtual void Emit(int numParticles);
    virtual void InitializeSystem();
    virtual ~ParticleSystem() noexcept;
private:
    void SetConstBuffer(UINT iLevel, UINT iLevelMask, UINT iWidth, UINT iHeight);

    // unused sample (future engine improvements, refactor and D3D12)
    HRESULT CreateStructuredBuffer(ID3D11Device* pDevice, UINT uElementSize, UINT uCount, void* pInitData, ID3D11Buffer** ppBufOut);
    HRESULT CreateRawBuffer(ID3D11Device* pDevice, UINT uSize, void* pInitData, ID3D11Buffer** ppBufOut);
    HRESULT CreateBufferSRV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11ShaderResourceView** ppSRVOut);
    HRESULT CreateBufferUAV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11UnorderedAccessView** ppUAVOut);
    //

protected:
    virtual void InitializeParticle(int index) = 0;
    void SortParticles();

    template<typename T>
    HRESULT CreateRWStructuredBuffer(ID3D11Device* device, ID3D11Buffer** buffer, UINT numElements, T* bufferData = nullptr)
    {
        UINT stride = (UINT)sizeof(T);
        UINT byteWidth = stride * numElements;

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = byteWidth;
        desc.Usage = D3D11_USAGE_DEFAULT; // without CPU write
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0u;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = stride;

        if (bufferData)
        {
            D3D11_SUBRESOURCE_DATA data;
            data.pSysMem = bufferData;
            data.SysMemPitch = 0;
            data.SysMemSlicePitch = 0;
            return device->CreateBuffer(&desc, &data, buffer);
        }

        return device->CreateBuffer(&desc, nullptr, buffer);
    }

    template<typename T>
    HRESULT CreateRWStructuredBufferCPUWrite(ID3D11Device* device, ID3D11Buffer** buffer, UINT numElements, T* bufferData = nullptr)
    {
        UINT stride = (UINT)sizeof(T);
        UINT byteWidth = stride * numElements;

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = byteWidth;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = stride;

        if (bufferData)
        {
            D3D11_SUBRESOURCE_DATA data;
            data.pSysMem = bufferData;
            data.SysMemPitch = 0;
            data.SysMemSlicePitch = 0;
            return device->CreateBuffer(&desc, &data, buffer);
        }

        return device->CreateBuffer(&desc, nullptr, buffer);
    }

    template<typename T>
    bool ApplyChanges(ID3D11DeviceContext* deviceContext, ID3D11Buffer* buffer, UINT numElements, T* data)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ThrowIfFailed(deviceContext->Map(buffer, 0u, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));

        CopyMemory(mappedResource.pData, data, sizeof(T) * numElements);
        deviceContext->Unmap(buffer, 0u);
        return true;
    }

    // Graphics context. Graphics object manages these resources.
    ID3D11Device* mDevice = nullptr;
    ID3D11DeviceContext* mDeviceContext = nullptr;

    UINT      maxParticles = 4096u;    // maximum number of particles in total
    int       numParticles = 0;        // current number of particles
    XMVECTOR  origin;                  // center of the particle system
    float     accumulatedTime;         // track when was last particle emitted
    XMVECTOR  force;                   // force (gravity, wind, etc.) acting on the system

    // main particle pool
    ID3D11Buffer* particleBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mParticleBufferUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mParticleBufferSRV;

    // alive list
    ID3D11Buffer* sortListBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mSortListBufferUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSortListBufferSRV;

    ID3D11Buffer* sortListBuffer2 = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mSortListBufferUAV2;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSortListBufferSRV2;

    // dead list
    ID3D11Buffer* deadListBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mDeadListBufferUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mDeadListBufferSRV;

    // indirect
    ID3D11Buffer* indirectArgsBuffer = nullptr;

    // to fill particle pool
    Particle injectionParticleData[injectionBufferSize];
    ID3D11Buffer* injectionBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mInjectionBufferSRV;

    GeometryShader mBillboardGeometryShader;
    ComputeShader mBitonicSortShader;
    ComputeShader mBitonicTransposeShader;

    // CS constant buffer
    ConstantBuffer<SortConstantBuffer> mCBSort;
    SortConstantBuffer mSortData;

    ConstantBuffer<ParticleConstantBuffer> mCBParticle;
    ParticleConstantBuffer mParticleData;

    ConstantBuffer<Camera—onstantBuffer> mCBCamera;
    Camera—onstantBuffer mCameraData;
    ThirdPersonCamera* camera = nullptr;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> mParticleSamplerClamp;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBillboardTexture;
};