#pragma once

#include "UploadBuffer.h"

struct FrameResource
{
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT pointLightsCount);
    FrameResource(const FrameResource& lhs) = delete;
    FrameResource& operator=(const FrameResource& lhs) = delete;

    ~FrameResource();

    ComPtr<ID3D12CommandAllocator> commandAllocator;

    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectsCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> MaterialSB = nullptr;
    std::unique_ptr<UploadBuffer<InstanceData>> PointLightSB = nullptr;
    
    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};