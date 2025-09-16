#define MaxLights 16

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

struct Light
{
    float3 Strength;
    float FallOfStart;
    float3 Direction;
    float FallOfEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess; // 1 - roughness
};

cbuffer cbPerMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPerPass : register(b2)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePos;
    float pad1;
    float gNearZ;
    float gFarZ;
    float gDeltaTime;
    float gTotalTime;
    
    float4 gAmbient;
    
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 pad2;
    
    Light gLights[MaxLights];
};

Texture2D gDiffuseMap : register(t0);

SamplerState gSamplerPointWrap : register(s0);
SamplerState gSamplerLinearWrap : register(s1);
SamplerState gSamplerAnisotropicWrap : register(s2);

struct PSInput
{
    float4 iPosH    : SV_POSITION;
    float3 iPosW    : POSITION0;
    float3 iNormalW : NORMAL;
    float2 iTexC    : TEXCOORD0;
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
        return 0.0f;
    
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
float4 ComputeLight(float3 N, float3 posW, float3 viewDir, Material mat)
{   
    float3 litColor = 0.0f;
    
#if NUM_DIR_LIGHTS
    
    float3 dirLight = 0.f;
    
    [unroll]
    for (int i = 0; i < NUM_DIR_LIGHTS; i++)
    {
        dirLight += CalcDirLight(gLights[i], N, viewDir, mat);
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

float4 main(PSInput input) : SV_TARGET
{
    // Sample diff albedo from texture and multiply by material diffuse albedo for some tweak if we need one (gDiffuseAlbedo = (1, 1, 1, 1) by default).
    float4 diffuseAlbedo = gDiffuseMap.Sample(gSamplerAnisotropicWrap, input.iTexC) * gDiffuseAlbedo;
    
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
    
    Material mat = { diffuseAlbedo, gFresnelR0, 1.0f - gRoughness };
    
    litColor += ComputeLight(N, input.iPosW, viewDir, mat);
    
    // linear fog
#ifdef FOG
    float fogAmount = saturate(distToEye - gFogStart) / gFogRange;
    litColor = (1-fogAmount) * litColor + fogAmount * gFogColor;
#endif
    
    // set the alpha channel of the diffuse material of the object itself
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}