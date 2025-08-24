cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
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
    
    output.oPosH = mul(float4(input.iPosL, 1.0f), gWorldViewProj);
    output.oColor = input.iColor;
	
    return output;
}