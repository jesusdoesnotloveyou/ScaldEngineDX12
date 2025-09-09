#define MaxLights 16

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
    float3 normal = normalize(N);
    
    float NdotL = max(dot(lightDirection, N), 0.0f);
    return NdotL * L.Strength;
}

float4 main(PSInput input) : SV_TARGET
{
    float4 ambient = gAmbient * gDiffuseAlbedo;
    float3 dirLight = CalcDirLight(gLights[0], input.iNormalW);
    
    float4 litColor = ambient + float4(dirLight, 0.0f);
    
    return litColor;
}