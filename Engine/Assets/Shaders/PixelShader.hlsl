struct PSInput
{
    float4 inPosition : SV_POSITION;
    float4 inColor : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
	return input.inColor;
}