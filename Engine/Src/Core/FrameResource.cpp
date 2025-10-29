#include "stdafx.h"
#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT pointLightsCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf())));

	ObjectsCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, TRUE);
	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, TRUE);
	MaterialSB = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, FALSE); // Structured buffer
	PointLightSB = std::make_unique<UploadBuffer<InstanceData>>(device, pointLightsCount, FALSE); // Structured buffer
}

FrameResource::~FrameResource() {}