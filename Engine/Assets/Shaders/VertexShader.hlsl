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
    float4 iColor : COLOR0;
    //float2 inTexCoord : TEXCOORD0;
    //float3 inNormal : NORMAL;
};

struct VSOutput
{
    float4 oPosH : SV_POSITION;
    float4 oColor : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.oPosH = mul(mul(float4(input.iPosL, 1.0f), gWorld), gViewProj);
    output.oColor = input.iColor;
	
    return output;
}