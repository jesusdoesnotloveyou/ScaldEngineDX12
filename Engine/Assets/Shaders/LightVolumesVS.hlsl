#include "Common.hlsl"

struct VSInput
{
    float3 iPosL    : POSITION;
    float3 iNormalL : NORMAL;
    float3 iTangent : TANGENT;
    float2 iTexC    : TEXCOORD;
};

struct VSOutput
{
    float4 oPosH    : SV_POSITION;
    float3 oPosW    : POSITION;
    float3 oNormalW : NORMAL;
    float2 oTexC    : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    
    float4 posW = mul(float4(input.iPosL, 1.0f), gWorld);
    output.oPosH = mul(posW, gViewProj);
    return output;
}