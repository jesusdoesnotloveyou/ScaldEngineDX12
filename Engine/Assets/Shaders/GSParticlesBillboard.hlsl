struct Particle
{
    float4 pos;
    float4 prevPos;
    float4 velocity;
    float4 acceleration;
    float4 initialColor;
    float4 endColor;
    
    float maxLifeTime;
    float lifeTime;
    float initialSize;
    float endSize;
    float initialWeight;
    float endWeight;
    float2 _pad;
};

cbuffer CameraData : register(b0)
{
    matrix view;
    matrix proj;
    float4 camForward;
    float4 camUp;
};

struct GS_IN
{
    uint particleIndex : INDEX;
};

struct GS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

StructuredBuffer<Particle> particlePool : register(t0);

[maxvertexcount(4)]
void main(point GS_IN input[1], inout TriangleStream<GS_OUT> triStream)
{
    Particle p = particlePool[input[0].particleIndex];

    float3 center = p.pos.xyz;
    float size = lerp(p.initialSize, p.endSize, p.lifeTime / p.maxLifeTime);
    float4 color = lerp(p.initialColor, p.endColor, p.lifeTime / p.maxLifeTime);

    float3 camRight = normalize(cross(camForward.xyz, camUp.xyz));

    // float3 forward = -normalize(view._13_23_33); 
    float3 up = normalize(view._12_22_32) * 0.5f * size;
    float3 right = normalize(view._11_21_31) * 0.5f * size;

    float3 positions[4] =
    {
        center - right + up,
        center + right + up,
        center - right - up,
        center + right - up
    };

    float2 uvs[4] =
    {
        float2(0, 0),
        float2(1, 0),
        float2(0, 1),
        float2(1, 1)
    };

    GS_OUT output = (GS_OUT) 0;

    // Order: 0-1-2-3
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        output.pos = mul(float4(positions[i], 1.0f), view);
        output.pos /= output.pos.w;
        output.pos = mul(output.pos, proj);
        //output.pos /= output.pos.w;
        output.uv = uvs[i];
        output.color = color;
        triStream.Append(output);
    }
 
    triStream.RestartStrip();
}