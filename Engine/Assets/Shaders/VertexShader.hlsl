struct VSInput
{
    float4 inPosition : POSITION0;
    float4 inColor : COLOR0;
    //float2 inTexCoord : TEXCOORD0;
    //float3 inNormal : NORMAL;
};

struct VSOutput
{
    float4 outPosition : SV_POSITION;
    float4 outColor : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.outPosition = input.inPosition;
    output.outColor = input.inColor;
	
    return output;
}