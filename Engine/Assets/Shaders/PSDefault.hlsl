#include "Common.hlsl"

struct PSInput
{
    float4 iPosH    : SV_POSITION;
    float3 iPosW    : POSITION0;
    float3 iNormalW : NORMAL;
    float2 iTexC    : TEXCOORD0;
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
    
    // !!!!!!!!! nothing really changes if you remove this if-statement below
    if (saturate(cascadePosH.x) == cascadePosH.x && saturate(cascadePosH.y) == cascadePosH.y)
    {
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
    
    return 1.0f.r;
}

float4 main(PSInput input) : SV_TARGET
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
    // Sample diff albedo from texture and multiply by material diffuse albedo for some tweak if we need one (gDiffuseAlbedo = (1, 1, 1, 1) by default).
    diffuseAlbedo *= gTextures[diffuseTexIndex].Sample(gSamplerAnisotropicWrap, input.iTexC);
    
    // To reject pixel as early as possible if it is completely transparent
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
    
    float3 N = normalize(input.iNormalW);
    
    float3 toEye = gEyePos - input.iPosW;
    float distToEye = length(toEye);
    float3 viewDir = toEye / distToEye;
    
    float4 ambient = gAmbient * diffuseAlbedo;
    float4 litColor = ambient;
    
    float viewDepth = mul(float4(input.iPosW, 1.0f), gView).z;
    
    Material mat = { diffuseAlbedo, fresnelR0, 1.0f - roughness };
    
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
    
    float3 cascadeColor = float3(0.0f, 0.0f, 0.0f);
    if (layer == 0)
        cascadeColor = float3(1.0f, 0.5f, 0.5f);
    else if (layer == 1)
        cascadeColor = float3(0.5f, 1.0f, 0.5f);
    else if (layer == 2)
        cascadeColor = float3(1.0f, 0.5f, 1.0f);
    else
        cascadeColor = float3(0.5f, 1.0f, 1.0f);
    
    float shadowFactor = GetShadowFactor(input.iPosW, layer);
    float3 dirLight = CalcDirLight(gDirLight, N, viewDir, mat, shadowFactor);
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