struct SortList
{
    uint index;
    float distanceSq;
};

StructuredBuffer<SortList> gSortListBuffer : register(t0);

struct VSOutput
{
    /*nointerpolation*/ uint oParticleIndex : INDEX;
};

VSOutput main(uint vid : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;
    output.oParticleIndex = gSortListBuffer[vid].index;
    return output;
}

Texture2D gBillboardTexture : register(t0);
SamplerState gParticleSampler : SAMPLER : register(s0);

struct PS_IN
{
    float4 iPosH  : SV_POSITION;
    float2 iTexC  : TEXCOORD0;
    float4 iColor : COLOR0;
};

float4 main(PS_IN input) : SV_TARGET
{
    return input.iColor * gBillboardTexture.Sample(gParticleSampler, input.iTexC);
}