#include "Common.hlsl"

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

struct PSInput
{
    float4 iPosH       : SV_POSITION;
    float3 iPosW       : POSITION0;
    float3 iNormalW    : NORMAL;
    float2 iTexC       : TEXCOORD0;
};

float3 SchlickApproximation(float3 fresnelR0, float3 halfVec, float3 lightDir)
{
    float cosIncidentAngle = saturate(dot(halfVec, lightDir)); // clamp between [0, 1] -> [0, 90] degrees
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflection = fresnelR0 + (1.0f - fresnelR0) * (f0 * f0 * f0 * f0 * f0);
    return reflection;
}

// Check F.Luna Introduction to DirectX12 paragraph 8.8 to see lighting model details.
float3 BlinnPhong(float3 lightStrength, float3 lightDir, float3 N, float3 viewDir, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    
    float3 halfVec = normalize(viewDir + lightDir);
    
    float3 schlick = SchlickApproximation(mat.FresnelR0, halfVec, lightDir);
    float normFactor = (m + 8.0f) / 8.0f;
    float nh = pow(max(dot(halfVec, N), 0.0f), m);
    
    float3 specAlbedo = schlick * normFactor * nh;
    
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);
    
    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 CalcDirLight(Light L, float3 N, float3 viewDir, Material mat)
{
    float3 lightDirection = normalize(-L.Direction);
    float NdotL = max(dot(lightDirection, N), 0.0f);
    float3 lightStrength = NdotL * L.Strength;
    return BlinnPhong(lightStrength, lightDirection, N, viewDir, mat);
}

float CalcAttenuation(float distance, float falloffStart, float falloffEnd)
{
    return saturate((falloffEnd - distance) / (falloffEnd - falloffStart));
}

float3 CalcPointLight(Light L, float3 N, float3 posW, float3 viewDir, Material mat)
{
    float3 lightVec = L.Position - posW;
    
    float d = length(lightVec);
    
    if (d > L.FallOfEnd)
        return .0f;
    
    float3 lightDir = lightVec / d;
    float NdotL = max(dot(lightDir, N), 0.0f);
    float attenuation = CalcAttenuation(d, L.FallOfStart, L.FallOfEnd);
    float3 lightStrength = L.Strength * NdotL * attenuation;
    
    return BlinnPhong(lightStrength, lightDir, N, viewDir, mat);
}

float3 CalcSpotLight(Light L, float3 N, float3 posW, float3 viewDir, Material mat)
{
    
    return float3(1.f, 1.f, 1.f);
}

// The idea is to calculate every light object's illumination strength related to it's type specific parameters
// and to pass it in BlinnPhong model function
float4 ComputeLight(float3 N, float3 posW, float3 viewDir, Material mat, float shadowFactor)
{   
    float3 litColor = 0.0f;
    
#if NUM_DIR_LIGHTS
    
    float3 dirLight = 0.f;
    
    [unroll]
    for (int i = 0; i < NUM_DIR_LIGHTS; i++)
    {
        // stuff with shadows supposed to work with only one directional light
        dirLight += shadowFactor * CalcDirLight(gLights[i], N, viewDir, mat);
    }
    litColor += dirLight;
#endif
    
#if NUM_POINT_LIGHTS
    
    float3 pointLight = 0.f;
    
    [unroll]
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i++)
    {
        pointLight += CalcPointLight(gLights[i], N, posW, viewDir, mat);

    }
    litColor += pointLight;
#endif
    
#if NUM_SPOT_LIGHTS
    
    float3 spotLight = 0.f;
    
    [unroll]
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; i++)
    {
        spotLight += CalcSpotLight(gLights[i], N, posW, viewDir, mat);
    }
    litColor += spotLight;
#endif
    
    return float4(litColor, 0.0f);
}

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
    
    return 1.0f;
}

float4 main(PSInput input) : SV_TARGET
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
    // Sample diff albedo from texture and multiply by material diffuse albedo for some tweak if we need one (gDiffuseAlbedo = (1, 1, 1, 1) by default).
    diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gSamplerAnisotropicWrap, input.iTexC);
    
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
    
    litColor += ComputeLight(N, input.iPosW, viewDir, mat, shadowFactor);
    
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