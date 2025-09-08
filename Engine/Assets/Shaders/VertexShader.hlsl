cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
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
};

struct VSInput
{
    float3 iPosL : POSITION0;
    //float2 inTexCoord : TEXCOORD0;
    float3 inNormalL : NORMAL;
};

struct VSOutput
{
    float4 oPosH    : SV_POSITION;
    float3 oPosW    : POSITION0;
    float3 oNormalW : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    
    float4 oPosW = mul(float4(input.iPosL, 1.0f), gWorld);
    
    output.oPosW = oPosW.xyz;
    output.oPosH = mul(oPosW, gViewProj);
    output.oNormalW = mul(input.inNormalL, (float3x3) gWorld);
	
    return output;
}