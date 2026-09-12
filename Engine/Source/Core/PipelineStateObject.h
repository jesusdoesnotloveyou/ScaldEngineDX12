#pragma once

#include "DXHelper.h"

namespace Scald
{
    // For now this class is final
    class PipelineStateObject final
    {
    public:
        PipelineStateObject(ID3D12Device* device);
        virtual ~PipelineStateObject();

        void Bind(ID3D12GraphicsCommandList* cmdList);
        void Unbind(ID3D12GraphicsCommandList* cmdList);

    private:
        ID3D12PipelineState* m_pipelineState;
    };
}