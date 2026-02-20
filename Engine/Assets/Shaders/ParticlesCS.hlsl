//#include "ComputeCommon.hlsl"

// num of threads within the thread group
[numthreads(16, 16, 1)]
void CSMain(uint GroupThreadID : SV_GroupThreadID, int3 DispatchThreadID : SV_DispatchThreadID)
{
    
    return;
}

cbuffer ParticlesData : register(b0)
{
    float deltaTime;
    uint maxNumParticles;
    uint numEmitInThisFrame;
    uint numAliveParticles;
    float4 gEmitPos;
    float4 gEyePos;
};

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

struct Sort
{
    uint index;
    float distanceSq;
};

RWStructuredBuffer<Particle> ParticlePool    : register(u0); // Read-Write structured buffer, has to be created on CPU side UnorderedAccessView
RWStructuredBuffer<Sort> SortList            : register(u1);         // Read-Write structured buffer, has to be created on CPU side UnorderedAccessView
AppendStructuredBuffer<uint> DeadList        : register(u2);     // Read-Write structured buffer, has to be created on CPU side UnorderedAccessView
ConsumeStructuredBuffer<uint> DeadListToInit : register(u3);

StructuredBuffer<Particle> InjectionBuffer   : register(t0);

float distanceSquared(float3 a, float3 b)
{
    float3 d = a - b;
    return dot(d, d);
}

#define THREAD_GROUP_X 32
#define THREAD_GROUP_Y 32
#define THREAD_GROUP_TOTAL 1024

[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void Simulate(uint3 DTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint threadGroupOffset = THREAD_GROUP_TOTAL * (Gid.x + Gid.y * 32);
    uint sortStructIndex = threadGroupOffset + DTid.y * 32 + DTid.x;

    if (sortStructIndex >= numAliveParticles || sortStructIndex >= maxNumParticles)
        return;

    Sort sls = SortList[sortStructIndex];
    uint particleIndex = sls.index;
    Particle p = ParticlePool[particleIndex];

    p.lifeTime += deltaTime;
    p.prevPos = p.pos;
    p.pos += p.velocity * deltaTime;
    p.velocity += p.acceleration * deltaTime;

    [branch]
    if (p.lifeTime >= p.maxLifeTime)
    {
        DeadList.Append(particleIndex);
        sls.distanceSq = 100000.0f;
        SortList.DecrementCounter();
    }
    else
    {
        sls.distanceSq = distanceSquared(p.pos.xyz, gEyePos.xyz);
    }
    
    ParticlePool[particleIndex] = p;
    SortList[sortStructIndex] = sls;
}

[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void Emit(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.y * THREAD_GROUP_Y + DTid.x;
    uint numDeadParticles = maxNumParticles - numAliveParticles;
    
    if (index >= numDeadParticles || index >= numEmitInThisFrame || index >= maxNumParticles)
        return;
    
    uint particleIndex = DeadListToInit.Consume();
    
    Particle p = InjectionBuffer[index];
    
    ParticlePool[particleIndex] = p;
    
    Sort sls;
    sls.index = particleIndex;
    sls.distanceSq = distanceSquared(p.pos.xyz, gEyePos.xyz);
    
    uint sortIndex = SortList.IncrementCounter();
    SortList[sortIndex] = sls;
}