// Defaults for number of lights.

// Forward Rendering
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 0
#endif
#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif
#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#define MaxCascades 4

#define TexturesCount 6

// Deferred Rendering
#define GBufferSize 6 // should be sync with GBuffer class

#ifndef G_DIFF_ALBEDO
    #define G_DIFF_ALBEDO 0
#endif
#ifndef G_AMB_OCCL
    #define G_AMB_OCCL 1
#endif
#ifndef G_NORMAL
    #define G_NORMAL 2
#endif
#ifndef G_SPECULAR
    #define G_SPECULAR 3
#endif
#ifndef G_MOTION_VEC
    #define G_MOTION_VEC 4
#endif
#ifndef G_DEPTH
    #define G_DEPTH 5
#endif

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

struct InstanceData
{
    float4x4 gWorld;
    Light gLight;
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
    
    uint NumDirLights = 0u;
    uint NumPointLights = 0u;
    
    Light gDirLight;
};

StructuredBuffer<InstanceData/*Light*/> gPointLights : register(t0, space1);
StructuredBuffer<InstanceData/*Light*/> gSpotLights : register(t1, space1);
Texture2D gGBuffer[GBufferSize] : register(t2, space1); // t2, t3, t4, t5, t6, t7 in space1

Texture2DArray gShadowMaps : register(t0);
StructuredBuffer<MaterialData> gMaterialData : register(t1);
TextureCube gCubeMap : register(t2);
Texture2D gDiffuseMap[TexturesCount] : register(t3); // t3, t4, t5, t6, t7, t8 in space0

SamplerState gSamplerPointWrap : register(s0);
SamplerState gSamplerLinearWrap : register(s1);
SamplerState gSamplerAnisotropicWrap : register(s2);
SamplerState gShadowSamplerLinearBorder : register(s3);
SamplerComparisonState gShadowSamplerComparisonLinearBorder : register(s4);

// Add in specular reflections.
float3 ComputeSpecularReflections(float3 toEyeW, float3 normalW, Material mat)
{
    float3 r = reflect(-toEyeW, normalW);
    float4 reflectionColor = gCubeMap.Sample(gSamplerAnisotropicWrap, r);
    float3 fresnelFactor = SchlickApproximation(mat.FresnelR0, normalW, r);
    return (mat.Shininess * fresnelFactor * reflectionColor.rgb);
}

float3 ComputeWorldPos(float3 texcoord)
{
    float depth = gGBuffer[G_DEPTH].Load(int3(texcoord)).r;

    float2 uv = texcoord.xy / gRTSize;
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y, depth, 1.0f);
    float4 worldPos = mul(ndc, gInvViewProj);
    worldPos.xyz /= worldPos.w;
    return worldPos.xyz;
}