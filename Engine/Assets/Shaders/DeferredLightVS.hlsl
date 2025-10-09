#include "Common.hlsl"

struct VSOutput
{
    float4 iPosH : SV_POSITION;
    float2 iTexC : TEXCOORD0;
};

VSOutput main(uint id: SV_VertexID)
{
    VSOutput output = (VSOutput) 0;

    output.iTexC = float2(id & 1, (id & 2) >> 1);
    output.iPosH = float4(output.iTexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}