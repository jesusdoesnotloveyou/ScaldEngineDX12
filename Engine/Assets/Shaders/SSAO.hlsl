#include "SSAOCommon.hlsl"

Texture2D RandVecMap : register(t2);

static const int gSampleCount = 14;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut SsaoVS(uint vid : SV_VertexID)
{
    VertexOut output = (VertexOut) 0;

    output.TexC = float2(vid & 1, (vid & 2) >> 1);
    // Quad covering screen in NDC space.
    output.PosH = float4(output.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    
    // Transform quad corners to view space near plane.
    float4 ph = mul(output.PosH, gInvProj);
    output.PosV = ph.xyz / ph.w;
    
    return output;
}

// Determines how much the sample point q occludes the point p as a function of distZ.
float OcclusionFunction(float distZ)
{
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \ 
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	
    float occlusion = 0.0f;
    if (distZ > gSurfaceEpsilon)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
		
		// Linearly decrease occlusion from 1 to 0 as distZ goes from gOcclusionFadeStart to gOcclusionFadeEnd.	
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
	
    return occlusion;
}

float4 SsaoPS(VertexOut input) : SV_Target
{
    // p -- the point we are computing the ambient occlusion for.
	// n (normalV) -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.
    
    // from GBuffer
    float3 normalW = gNormalMap.Sample(gSamplerPointClamp, input.TexC).xyz;
    float pz = gDepthMap.Sample(gSamplerDepthMap, input.TexC).r;
    
    pz = NdcDepthToViewDepth(pz);
    
    float3 normalV = mul(normalW, (float3x3) gView);
    
    // Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
    float3 p = (pz / input.PosV.z) * input.PosV;

    // Extract random vector and map from [0,1] --> [-1, +1].
    float3 randVec = 2.0f * RandVecMap.Sample(gSamplerLinearWrap, 4.0f * input.TexC).rgb - 1.0f;

    float occlusionSum = 0.0f;
    
    // Sample neighboring points about p in the hemisphere oriented by n.
    for (int i = 0; i < gSampleCount; ++i)
    {
        // Are offset vectors are fixed and uniformly distributed (so that our offset vectors do not clump in the same direction). 
        // If we reflect them about a random vector then we get a random uniform distribution of offset vectors.
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
	
		// Flip offset vector if it is behind the plane defined by (p, n).
        float flip = sign(dot(offset, normalV));
		
		// Sample a point near p within the occlusion radius.
        float3 q = p + flip * gOcclusionRadius * offset;
		
		// Project q and generate projective tex-coords.  
        float4 projQ = mul(float4(q, 1.0f), gProjTex);
        projQ /= projQ.w;

        // Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might occupy empty space).
        // To find the nearest depth we look it up in the depthmap.
        float rz = gDepthMap.SampleLevel(gSamplerDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);
        
        // Reconstruct full view space position r = (rx,ry,rz).  We know r
		// lies on the ray of q, so there exists a t such that r = t*q.
		// r.z = t*q.z ==> t = r.z / q.z
        float3 r = (rz / q.z) * q;
        
        // Test whether r occludes p.
		//   * The product dot(n, normalize(r - p)) measures how much in front
		//     of the plane(p,n) the occluder point r is.  The more in front it is, the
		//     more occlusion weight we give it.  This also prevents self shadowing where 
		//     a point r on an angled plane (p,n) could give a false occlusion since they
		//     have different depth values with respect to the eye.
		//   * The weight of the occlusion is scaled based on how far the occluder is from
		//     the point we are computing the occlusion of.  If the occluder r is far away
		//     from p, then it does not occlude it.
		
        float distZ = p.z - r.z;
        float dp = max(dot(normalV, normalize(r - p)), 0.0f);

        float occlusion = dp * OcclusionFunction(distZ);

        occlusionSum += occlusion;
    }
    
    occlusionSum /= gSampleCount;
    
    float access = 1.0f - occlusionSum;
    // Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
    return saturate(pow(access, 6.0f));
}