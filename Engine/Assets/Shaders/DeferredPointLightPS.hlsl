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
    return 1.0f;
}
