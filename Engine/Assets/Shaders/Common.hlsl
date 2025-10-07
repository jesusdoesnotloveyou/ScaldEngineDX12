#include "LightUtil.hlsl"

#define MaxCascades 4

struct CascadesShadows
{
    float4x4 CascadeViewProj[MaxCascades];
    float4 Distances;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint pad0;
    uint pad1;
    uint pad2;
};

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    CascadesShadows gCascadeData;
    float3 gEyePos;
    float gPassPad0;
    float gNearZ;
    float gFarZ;
    float gDeltaTime;
    float gTotalTime;
    
    float4 gAmbient;
    
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 gPassPad1;
    
    Light gLights[MaxLights];
};

Texture2DArray gShadowMaps : register(t0);
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

Texture2D gDiffuseMap[6/*magic hardcode*/] : register(t1); // t1, t2, t3, t4, t5, t6 in space0


SamplerState gSamplerPointWrap : register(s0);
SamplerState gSamplerLinearWrap : register(s1);
SamplerState gSamplerAnisotropicWrap : register(s2);
SamplerState gShadowSamplerLinearBorder : register(s3);
SamplerComparisonState gShadowSamplerComparisonLinearBorder : register(s4);