#include "PipelineStateObject.h"

using namespace Scald;

PipelineStateObject::PipelineStateObject(ID3D12Device* device)
{
    // Initialize the pipeline state object here
}

PipelineStateObject::~PipelineStateObject()
{
    // Cleanup resources here
}

void PipelineStateObject::Bind(ID3D12GraphicsCommandList* cmdList)
{
    // Bind the pipeline state object here
}

void PipelineStateObject::Unbind(ID3D12GraphicsCommandList* cmdList)
{
    // Unbind the pipeline state object here
}
