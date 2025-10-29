#include "Common.hlsl"

struct VSInput
{
    float3 iPosL     : POSITION0;
    float3 iNormalL : NORMAL;
    float3 iTangent  : TANGENT;
    float2 inTexC    : TEXCOORD0;
};

struct VSOutput
{
    float4 oPosH : POSITION0;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    output.oPosH = mul(float4(input.iPosL, 1.0f), gWorld);
    return output;
}