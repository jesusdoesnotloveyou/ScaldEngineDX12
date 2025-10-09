#include "Common.hlsl"

struct VSInput
{
    float3 iPosL     : POSITION0;
    float3 inNormalL : NORMAL;
    float2 inTexC    : TEXCOORD0;
};

struct VSOutput
{
    float4 oPosH    : SV_POSITION;
    float3 oPosW    : POSITION0;
    float3 oNormalW : NORMAL;
    float2 oTexC    : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;

    return output;
}