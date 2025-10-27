#include "Common.hlsl"

struct PSInput
{
    float4 iPosH    : SV_POSITION;
    float3 iPosW    : POSITION;
    float3 iNormalW : NORMAL;
    float2 iTexC    : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 texCoord = input.iPosH.xy;
    
    float4 diffuseAlbedo = gGBuffer[G_DIFF_ALBEDO].Load(input.iPosH.xyz);
    float4 ambientOcclusion = gGBuffer[G_AMB_OCCL].Load(input.iPosH.xyz);
    float4 normalTex = gGBuffer[G_NORMAL].Load(input.iPosH.xyz);
    float4 specularTex = gGBuffer[G_SPECULAR].Load(input.iPosH.xyz);
    float3 posW = ComputeWorldPos(float3(texCoord, 0.0f));
    
    float3 fresnelR0 = specularTex.xyz;
    float shininess = exp2(specularTex.a * 10.5f);
    
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    
    float3 N = normalize(normalTex.xyz);
    float3 toEye = gEyePos - posW;
    float distToEye = length(toEye);
    float3 viewDir = toEye / distToEye;
    
    float3 pointLight = CalcPointLight(gDirLight, N, posW, viewDir, mat);
    return float4(pointLight, 1.0f);
}