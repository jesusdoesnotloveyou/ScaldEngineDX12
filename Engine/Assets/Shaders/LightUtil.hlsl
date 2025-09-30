#define MaxLights 16

struct Light
{
    float3 Strength;
    float FallOfStart;
    float3 Direction;
    float FallOfEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess; // 1 - roughness
};