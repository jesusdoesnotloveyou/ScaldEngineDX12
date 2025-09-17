
// the geometry shader is invoked per primitive

#ifndef INSTANCE_COUNT
    #define INSTANCE_COUNT 4
#endif

struct GSInput
{
    float3 iPosW    : POSITION0;
    float3 iNormalW : NORMAL;
    float2 iTexC    : TEXCOORD0;
    
};

struct GSOutput
{
    float4 oPosH    : SV_POSITION;
    float3 iPosW    : POSITION0;
    float3 oNormalW : NORMAL;
    float2 oTexC    : TEXCOORD0;
    uint   arrInd   : SV_RenderTargetArrayIndex;
};

[instance(INSTANCE_COUNT)]
//the maximum number of vertices the geometry shader will output for a single invocation
[maxvertexcount(3)]
void main(triangle GSInput p[3], in uint id : SV_GSInstanceID, inout TriangleStream<GSOutput> stream)
{
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        GSOutput output = (GSOutput) 0;
        
        
        output.arrInd = id;
        stream.Append(output);
    }
}