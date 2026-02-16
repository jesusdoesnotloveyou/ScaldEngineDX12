cbuffer cbSsao : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;
    float4 gOffsetVectors[14];

    // For SsaoBlur.hlsl
    float4 gBlurWeights[3];

    float2 gInvRenderTargetSize;

    // Coordinates given in view space.
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEpsilon;
};

cbuffer cbRootConstants : register(b1)
{
    bool gHorizontalBlur;
};

Texture2D gNormalMap : register(t0);
Texture2D gDepthMap : register(t1);
Texture2D gInputMap : register(t2);

SamplerState gSamplerPointClamp : register(s0);
SamplerState gSamplerLinearClamp : register(s1);
SamplerState gSamplerDepthMap : register(s2);
SamplerState gSamplerLinearWrap : register(s3);

static const int gBlurRadius = 5;

static const int gSampleCount = 14;

// vertices id's
static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct VSOutput
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOutput SsaoBlurVS(uint vid : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;
    
    output.TexC = gTexCoords[vid];
    // Quad covering screen in NDC space.
    output.PosH = float4(2.0f * output.TexC.x - 1.0f, 1.0f - 2.0f * output.TexC.y, 0.0f, 1.0f);
    
    return output;
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
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
	 
    float3 centerNormal = gNormalMap.SampleLevel(gSamplerPointClamp, input.TexC, 0.0f).xyz;
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