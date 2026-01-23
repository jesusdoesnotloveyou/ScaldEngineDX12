#include "Common.hlsl"

struct PSInput
{
    float4 iPosH : SV_POSITION;
    
    nointerpolation uint iInstanceID : InstanceID;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 texCoord = input.iPosH.xy;
    
    InstanceData instData = gPointLights[input.iInstanceID];
    
    GBufferPixelData gbuffer = FetchGBufferData(input.iPosH);
    
    float3 posW = ComputeWorldPos(float3(texCoord, 0.0f));
    
    float3 fresnelR0 = gbuffer.specular.xyz;
    const float shininess = exp2(gbuffer.specular.a * 10.5f) * gbuffer.normal.a;
    
    Material mat = { gbuffer.diffuse, fresnelR0, shininess };
    
    float3 toEye = gEyePos - posW;
    float3 viewDir = toEye / length(toEye);
    
    float3 pointLight = CalcPointLight(instData.gLight, gbuffer.normal.xyz, posW, viewDir, mat);
    
    // Does not work properly
    //pointLight += ComputeSpecularReflections(toEye, N, mat);
    
    return float4(pointLight, 1.0f);
}