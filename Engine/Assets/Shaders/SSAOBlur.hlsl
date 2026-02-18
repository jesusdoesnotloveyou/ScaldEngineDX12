#include "SSAOCommon.hlsl"

Texture2D gInputMap : register(t2);

static const int gBlurRadius = 5;

struct VSOutput
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOutput SsaoBlurVS(uint vid : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;
    
    output.TexC = float2(vid & 1, (vid & 2) >> 1);
    // Quad covering screen in NDC space.
    output.PosH = float4(output.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    
    return output;
}

float4 SsaoBlurPS(VSOutput input) : SV_Target
{
    // unpack into float array.
    float blurWeights[12] =
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };

    float2 texOffset;
    if (gHorizontalBlur)
    {
        texOffset = float2(gInvRenderTargetSize.x, 0.0f);
    }
    else
    {
        texOffset = float2(0.0f, gInvRenderTargetSize.y);
    }

	// The center value always contributes to the sum.
    float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gSamplerPointClamp, input.TexC, 0.0);
    float totalWeight = blurWeights[gBlurRadius];
	 
    float3 centerNormal = mul(gNormalMap.SampleLevel(gSamplerPointClamp, input.TexC, 0.0f).xyz, (float3x3) gView);
    float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gSamplerDepthMap, input.TexC, 0.0f).r);

    for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
		// We already added in the center weight.
        if (i == 0) continue;

        float2 tex = input.TexC + i * texOffset;

        float3 neighborNormal = gNormalMap.SampleLevel(gSamplerPointClamp, tex, 0.0f).xyz;
        float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gSamplerDepthMap, tex, 0.0f).r);

		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
	
        if (dot(neighborNormal, centerNormal) >= 0.8f &&
		    abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = blurWeights[i + gBlurRadius];

			// Add neighbor pixel to blur.
            color += weight * gInputMap.SampleLevel(gSamplerPointClamp, tex, 0.0);
		
            totalWeight += weight;
        }
    }

	// Compensate for discarded samples by making total weights sum to 1.
    return color / totalWeight;
}