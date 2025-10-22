#include "Common.hlsl"

struct PSInput
{
    float4 iPosH : SV_POSITION;
    float2 iTexC : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    
    
    return 1.0f;
}