#include "Common.hlsl"

struct PSInput
{
    float4 iPosH     : SV_POSITION;
    float3 iPosW     : POSITION0;
    float3 iNormalW  : NORMAL;
    float3 iTangentW : TANGENT;
    float2 iTexC     : TEXCOORD0;
};

struct GBuffer
{
    float4 DiffuseAlbedo     : SV_Target0;
    float4 AmbientOcclusion  : SV_Target1;
    float4 Normal            : SV_Target2; // could be used only 2 components instead of 4
    float4 Specular          : SV_Target3;
    float2 MotionVectors     : SV_Target4;
};

[earlydepthstencil]
GBuffer main(PSInput input)
{
    GBuffer output = (GBuffer) 0;
    
    MaterialData matData = gMaterialData[gMaterialIndex];
    
    // Interpolating normal can unnormalize it, so renormalize it.
    input.iNormalW = normalize(input.iNormalW);
    
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;
    
    diffuseAlbedo *= gTextures[diffuseMapIndex].Sample(gSamplerAnisotropicWrap, input.iTexC);
    output.DiffuseAlbedo = diffuseAlbedo;

    output.AmbientOcclusion = float4(input.iPosW, 0.0f); // temporary

    output.Normal = float4(input.iNormalW, 0.0f);
    
    if (normalMapIndex != INVALID_INDEX)
    {
        float4 normalMapSample = gTextures[512 + normalMapIndex].Sample(gSamplerAnisotropicWrap, input.iTexC);
        output.Normal.xyz = NormalSampleToWorldSpace(normalMapSample.rgb, input.iNormalW, input.iTangentW);
        output.Normal.a = normalMapSample.a;
    }
    
    output.Specular = float4(fresnelR0, log2(1.0f - roughness) / 10.5f);
    // TO DO: prev and curr frames camera diffs
    output.MotionVectors;
    
    return output;
}