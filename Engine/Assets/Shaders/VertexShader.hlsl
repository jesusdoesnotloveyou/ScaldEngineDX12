cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
}

struct VSInput
{
    float4 iPosL : POSITION0;
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
    
    output.oPosH = input.iPosL;
    output.oColor = input.iColor;
	
    return output;
}