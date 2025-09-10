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

cbuffer cbPerMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float Roughness;
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
    
    Light gLights[MaxLights];
};

struct PSInput
{
    float4 iPosH    : SV_POSITION;
    float3 iPosW    : POSITION0;
    float3 iNormalW : NORMAL;
};

float3 CalcDirLight(Light L, float3 N)
{
    float3 lightDirection = normalize(-L.Direction);
    
    float NdotL = max(dot(lightDirection, N), 0.0f);
    return NdotL * L.Strength;
}

float CalcAttenuation(float distance, float falloffStart, float falloffEnd)
{
    return saturate((falloffEnd - distance) / (falloffEnd - falloffStart));
}

float3 CalcPointLight(Light L, float3 N, float3 posW, float3 toEye)
{
    float3 lightVec = L.Position - posW;
    
    float d = length(lightVec);
    
    if (d > L.FallOfEnd)
        return 0.0f;
    
    float3 lightDir = lightVec / d;
    float NdotL = max(dot(lightDir, N), 0.0f);
    float attenuation = CalcAttenuation(d, L.FallOfStart, L.FallOfEnd);
    
    return L.Strength * NdotL * attenuation;
}

float3 CalcSpotLight(Light L, float3 N, float3 posW, float3 toEye)
{
    
    return float3(1.f, 1.f, 1.f);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.iNormalW);
    
    float4 ambient = gAmbient * gDiffuseAlbedo;
    float4 litColor = ambient;
    
#if NUM_DIR_LIGHTS
    
    float3 dirLight = float3(0.f, 0.f, 0.f);
    
    [unroll]
    for (int i = 0; i < NUM_DIR_LIGHTS; i++)
    {
        dirLight += CalcDirLight(gLights[i], N);
    }
    litColor += float4(dirLight, 0.0f);
#endif
    
#if NUM_POINT_LIGHTS
    
    float3 pointLight = float3(0.f, 0.f, 0.f);
    
    [unroll]
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i++)
    {
        pointLight += CalcPointLight(gLights[i], N, input.iPosW, gEyePos);
    }
    litColor += float4(pointLight, 0.0f);
#endif
    
#if NUM_SPOT_LIGHTS
    
    float3 spotLight = float3(0.f, 0.f, 0.f);
    
    [unroll]
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; i++)
    {
        spotLight += CalcSpotLight(gLights[i], N, input.iPosW, gEyePos);
    }
    litColor += float4(spotLight, 0.0f);
#endif
    return litColor;
}