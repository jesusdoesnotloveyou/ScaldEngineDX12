#include "Common.hlsl"

struct PSInput
{
    float4 iPosH : SV_POSITION;
    float2 iTexC : TEXCOORD0;
};

float SampleShadowMap(uint layer, float2 uv, float depth)
{
    return gShadowMaps.SampleCmp(gShadowSamplerComparisonLinearBorder, float3(uv, layer), depth).r;
}

float GetShadowFactor(float3 posW, uint layer)
{
    //                                           cascade's view * proj
    float4 cascadePosH = mul(float4(posW, 1.0f), gCascadeData.CascadeViewProj[layer]);
    // to NDC
    cascadePosH.xyz /= cascadePosH.w;
    cascadePosH.xy = cascadePosH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    // depth in NDC
    float depth = cascadePosH.z;
    
    uint width, height, elements, levels;
    gShadowMaps.GetDimensions(0u, width, height, elements, levels);
    
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    
    const float2 offsets[9] = // we're in texCoord, imagine a pixel on texture and 8 pixel around it so they make a square
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += SampleShadowMap(layer, cascadePosH.xy + offsets[i], depth);
    }
    
    return percentLit / 9.0f;
}

float3 ComputeWorldPos(float3 texcoord)
{
    float depth = gGBuffer[4].Load(int3(texcoord)).r;

    float2 uv = texcoord.xy / gRTSize;
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y, depth, 1.0f);
    float4 worldPos = mul(ndc, gInvViewProj);
    worldPos.xyz /= worldPos.w;
    return worldPos.xyz;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 texCoord = input.iPosH.xy;
    float4 diffuseAlbedo = gGBuffer[0].Load(input.iPosH.xyz);
    float4 ambientOcclusion = gGBuffer[1].Load(input.iPosH.xyz);
    float4 normalTex = gGBuffer[2].Load(input.iPosH.xyz);
    float4 specularTex = gGBuffer[3].Load(input.iPosH.xyz);
    float3 posW = ComputeWorldPos(float3(texCoord, 0.0f));
    
    float3 fresnelR0 = specularTex.xyz;
    float shininess = exp2(specularTex.a * 10.5f);
    
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
    
    float3 N = normalize(normalTex.xyz);
    
    float3 toEye = gEyePos - posW;
    float distToEye = length(toEye);
    float3 viewDir = toEye / distToEye;
    
    float4 litColor = gAmbient;
    
    float viewDepth = mul(float4(posW, 1.0f), gView).z;
    
    uint layer = MaxCascades - 1;
    
    [unroll]
    for (uint i = 0; i < MaxCascades; ++i)
    {
        if (viewDepth < gCascadeData.Distances[i])
        {
            layer = i;
            break;
        }
    }
    
#ifdef SHADOW_DEBUG
    float3 cascadeColor = float3(0.0f, 0.0f, 0.0f);
    if (layer == 0)
        cascadeColor = float3(1.0f, 0.5f, 0.5f);
    else if (layer == 1)
        cascadeColor = float3(0.5f, 1.0f, 0.5f);
    else if (layer == 2)
        cascadeColor = float3(1.0f, 0.5f, 1.0f);
    else
        cascadeColor = float3(0.5f, 1.0f, 1.0f);
#endif
    
    float shadowFactor = GetShadowFactor(posW, layer);
    float3 dirLight = CalcDirLight(gLights[0], N, viewDir, mat, shadowFactor);
    litColor += float4(dirLight, 0.0f);
    
    // linear fog
#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    litColor = (1-fogAmount) * litColor + fogAmount * gFogColor;
#endif
    
    // set the alpha channel of the diffuse material of the object itself
    litColor.a = diffuseAlbedo.a;
    
#ifdef SHADOW_DEBUG
    return litColor * float4(cascadeColor, 1.0f);
#endif
    return litColor;
}