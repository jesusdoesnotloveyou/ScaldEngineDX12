
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