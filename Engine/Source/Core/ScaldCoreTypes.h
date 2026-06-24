#pragma once

#include <DirectXColors.h>

#include <random>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

constexpr int INVALID_ID = -1;
using ID_TYPE = int;

using MeshID = ID_TYPE;
using ModelID = ID_TYPE;
using TextureID = ID_TYPE;

enum class EPassType : UINT
{
    // ComputePass
    // ZPrePass
    DepthShadow = 0,  // first element in pass cbv contains data for depth pass
    // SSAO
    DeferredGeometry,  // second element in pass cbv contains data for geometry pass
    DeferredLighting,  // third element in pass cbv contains data for color/light pass
    NumPasses = 3
};

#define MaxCascades 4

#define MaxDirLights 1u
#define MaxPointLights 2048u
#define MaxSpotLights 2048u
#define MaxLightsPool (MaxPointLights + MaxSpotLights)

struct CascadesShadows
{
    CascadesShadows()
    {
        for (int i = 0; i < MaxCascades; ++i)
        {
            CascadeViewProj[i] = DirectX::XMMatrixIdentity();
            Distances[i] = 0.0f;
        }
    }

    DirectX::XMMATRIX CascadeViewProj[MaxCascades];
    float Distances[MaxCascades];
};

struct LightData
{
    DirectX::XMFLOAT3 Strength = {0.5f, 0.5f, 0.5f};
    float FallOfStart = 1.0f;                           // spot/point
    DirectX::XMFLOAT3 Direction = {0.5f, -1.0f, 0.5f};  // spot/dir
    float FallOfEnd = 10.0f;                            // spot/point
    DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};    // spot/point
    float SpotPower = 64.0f;                            // spot only
};

struct LightInstanceData
{
    DirectX::XMFLOAT4X4 World;
    LightData Light;
};

// Constant buffers
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 InvTransposeWorld;
    DirectX::XMFLOAT4X4 TexTransform;
    UINT MaterialIndex = 0u;
    UINT objPad0 = 0u;
    UINT objPad1 = 0u;
    UINT objPad2 = 0u;
};

struct InstanceData
{
    DirectX::XMFLOAT4X4 World;
    LightData Light;
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Proj;
    DirectX::XMFLOAT4X4 ViewProj;
    DirectX::XMFLOAT4X4 InvViewProj;

    CascadesShadows Cascades;

    DirectX::XMFLOAT3 EyePosW = {0.0f, 0.0f, 0.0f};
    float pad1 = 0.0f;

    DirectX::XMFLOAT2 RenderTargetSize = {0.0f, 0.0f};
    DirectX::XMFLOAT2 InvRenderTargetSize = {0.0f, 0.0f};

    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float DeltaTime = 0.0f;
    float TotalTime = 0.0f;

    DirectX::XMFLOAT4 Ambient = {0.0f, 0.0f, 0.0f, 1.0f};

    DirectX::XMFLOAT4 FogColor = {0.7f, 0.7f, 0.7f, 1.0f};
    float FogStart = 8.0f;
    float FogRange = 18.0f;

    uint32_t NumDirLights = 0u;
    uint32_t NumPointLights = 0u;

    LightData DirLight;
};

struct SSAOConstants
{
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Proj;
    DirectX::XMFLOAT4X4 InvProj;
    DirectX::XMFLOAT4X4 ProjTex;
    DirectX::XMFLOAT4 OffsetVectors[14];

    // For SSAOBlur.hlsl
    DirectX::XMFLOAT4 BlurWeights[3];

    DirectX::XMFLOAT2 InvRenderTargetSize = {0.0f, 0.0f};

    // Coordinates given in view space.
    float OcclusionRadius = 0.5f;
    float OcclusionFadeStart = 0.2f;
    float OcclusionFadeEnd = 2.0f;
    float SurfaceEpsilon = 0.05f;
};

// Structured buffers
struct MaterialData
{
    DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};
    float Roughness = 0.25f;
    DirectX::XMFLOAT4X4 MatTransform;
    UINT DiffuseMapIndex = 0u;
    UINT NormalMapIndex = 0u;
    UINT matPad1 = 0u;
    UINT matPad2 = 0u;
};

// Fprward Rendering

/*
 *	Particles
 */

// struct Particle
//{
//	XMFLOAT4 Color;
//
//	XMFLOAT3 Position;
//	float Age;
//
//	XMFLOAT3 Velocity;
//	float Size;
//
//	float Alive;
//	XMFLOAT3 pad;
// };

struct Particle
{
    DirectX::XMVECTOR pos           = DirectX::XMVectorZero();
    DirectX::XMVECTOR prevPos       = DirectX::XMVectorZero();
    DirectX::XMVECTOR velocity      = DirectX::XMVectorZero();
    DirectX::XMVECTOR acceleration  = DirectX::XMVectorZero();
    DirectX::XMVECTOR initialColor  = DirectX::XMVectorZero();
    DirectX::XMVECTOR endColor      = DirectX::XMVectorZero();
    float maxLifeTime = 1.0f;
    float lifeTime = 0.0f;
    float initialSize = 1.0f;
    float endSize = 1.0f;  // or sizeDelta
    float initialWeight = 1.0f;
    float endWeight = 1.0f;  // or weightDelta
    float pad[2];
};

struct ParticleConstantBuffer
{
    float deltaTime = 0.0f;
    UINT maxNumParticles = 0u;
    UINT numEmitInThisFrame = 0u;
    UINT numAliveParticles = 0u;
    DirectX::XMVECTOR gEmitPos = DirectX::XMVectorZero();
    DirectX::XMVECTOR gEyePos = DirectX::XMVectorZero();
};

struct CameraConstantBuffer
{
    DirectX::XMMATRIX gView = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX gProjection = DirectX::XMMatrixIdentity();
    DirectX::XMVECTOR gForward = DirectX::XMVectorZero();
    DirectX::XMVECTOR gUp = DirectX::XMVectorZero();
};

struct SortList
{
    UINT index = 0u;
    float distanceSq = std::numeric_limits<float>().max();
};

struct SortConstantBuffer
{
    UINT g_iLevel;
    UINT g_iLevelMask;
    UINT g_iWidth;
    UINT g_iHeight;
};

template <typename T>
T generateRandom(T min, T max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(min, max);
    return dist(gen);
}