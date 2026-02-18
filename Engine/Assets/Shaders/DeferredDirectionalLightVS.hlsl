#include "Common.hlsl"

struct VSOutput
{
    float4 oPosH     : SV_POSITION;
    float4 oSSAOPosH : POSITION;
    float2 oTexC     : TEXCOORD0;
};

VSOutput main(uint id: SV_VertexID)
{
    VSOutput output = (VSOutput) 0;

    output.oTexC = float2(id & 1, (id & 2) >> 1);
    output.oPosH = float4(output.oTexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    // TO DO: Might be changed somehow (probably move all calculation to pixel shader)
    output.oSSAOPosH = float4(output.oPosH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f), 0.0f, 1.0f);
    
    return output;
}