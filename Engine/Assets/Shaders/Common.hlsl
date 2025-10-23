// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#define MaxCascades 4
#define GBufferSize 5 // should be sync with GBuffer class

#include "LightUtil.hlsl"

struct CascadesShadows
{
    float4x4 CascadeViewProj[MaxCascades];
    float4 Distances;
};

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvTransposeWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gNormalMapIndex; // todo
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
    float2 gRTSize;
    float2 gInvRTSize;
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
Texture2D gGBuffer[GBufferSize] : register(t1, space1); // t1, t2, t3, t4, t5 in space1
Texture2D gDiffuseMap[6/*magic hardcode*/] : register(t1); // t1, t2, t3, t4, t5, t6 in space0


SamplerState gSamplerPointWrap : register(s0);
SamplerState gSamplerLinearWrap : register(s1);
SamplerState gSamplerAnisotropicWrap : register(s2);
SamplerState gShadowSamplerLinearBorder : register(s3);
SamplerComparisonState gShadowSamplerComparisonLinearBorder : register(s4);