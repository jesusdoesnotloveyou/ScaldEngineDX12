struct PSInput
{
    float4 iPosH    : SV_POSITION;
    float3 iPosW    : POSITION0;
    float3 iNormalW : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    return float4(input.iNormalW, 1.0f);
}