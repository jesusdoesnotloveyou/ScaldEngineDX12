#include "Common.hlsl"

struct VSInput
{
    float3 iPosL : POSITION0;
};

struct VSOutput
{
    float4 oPosH : SV_POSITION;
    // for instancing (not sure that's the best way to implement this logic)
    nointerpolation uint oInstanceID : InstanceID;
};

/// <summary>
/// If you look carefully at this shader and VSDeferredPL.hlsl, you'll notice that they're almost the same.
/// The reason they were still divided is just convenience since point lights and spot lights use different geometry primitives
/// (spheres and cones correspondingly) and different InstanceData structs.
/// </summary>

VSOutput main(VSInput input, uint instanceID : SV_InstanceID)
{
    VSOutput output = (VSOutput) 0;
    
    InstanceData instData = gSpotLights[instanceID];
    
    float4 posW = mul(float4(input.iPosL, 1.0f), instData.gWorld);
    output.oPosH = mul(posW, gViewProj);
    output.oInstanceID = instanceID;
    return output;
}