#define BITONIC_BLOCK_SIZE 512

#define TRANSPOSE_BLOCK_SIZE 16

cbuffer CB : register(b0)
{
    uint g_iLevel;
    uint g_iLevelMask;
    uint g_iWidth;
    uint g_iHeight;
};

struct ParticleDepth
{
    uint Index;
    float Depth;
};

RWStructuredBuffer<ParticleDepth> Data : register(u0);

groupshared ParticleDepth shared_data[BITONIC_BLOCK_SIZE];

[numthreads(BITONIC_BLOCK_SIZE, 1, 1)]
void BitonicSort(uint3 Gid : SV_GroupID,
                  uint3 DTid : SV_DispatchThreadID,
                  uint3 GTid : SV_GroupThreadID,
                  uint GI : SV_GroupIndex)
{
    // Load shared data
    shared_data[GI] = Data[DTid.x];
    GroupMemoryBarrierWithGroupSync();
    
    // Sort the shared data
    for (unsigned int j = g_iLevel >> 1; j > 0; j >>= 1)
    {
        uint ixj = GI ^ j;
        if (ixj > GI)
        {
            ParticleDepth a = shared_data[GI];
            ParticleDepth b = shared_data[ixj];

            bool ascending = ((DTid.x & g_iLevelMask) == 0);
            if ((a.Depth > b.Depth) == ascending)
            {
                shared_data[GI] = b;
                shared_data[ixj] = a;
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    // Store shared data
    Data[DTid.x] = shared_data[GI];
}

RWStructuredBuffer<ParticleDepth> Data2 : register(u1);

groupshared ParticleDepth transpose_shared_data[TRANSPOSE_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE];

[numthreads(TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE, 1)]
void MatrixTranspose(uint3 Gid : SV_GroupID,
          uint3 DTid : SV_DispatchThreadID,
          uint3 GTid : SV_GroupThreadID,
          uint GI : SV_GroupIndex)
{
    transpose_shared_data[GI] = Data[DTid.y * g_iWidth + DTid.x];
    GroupMemoryBarrierWithGroupSync();
    uint2 XY = DTid.yx - GTid.yx + GTid.xy;
    Data2[XY.y * g_iHeight + XY.x] = transpose_shared_data[GTid.x * TRANSPOSE_BLOCK_SIZE + GTid.y];
}