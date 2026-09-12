#pragma once

#include "DXHelper.h"
#include <string>

namespace Scald
{
    class Shader
    {
    public:
        Shader(const char* name, const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
        virtual ~Shader();

        void Bind(ID3D12GraphicsCommandList* cmdList);
        void Unbind(ID3D12GraphicsCommandList* cmdList);

    private:
        std::string m_name;
        std::wstring m_filename;
        ComPtr<ID3D12Resource> m_resource;
    };
}