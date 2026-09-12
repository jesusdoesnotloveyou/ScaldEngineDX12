#pragma once

#include "DXHelper.h"
#include "FileSystemObject.h"

#include <vector>

namespace Scald
{
    class Mesh;
    class Material;

    class Model final : public FileSystemObject
    {
    public:
        Model(const Path& relativePath);
        ~Model() noexcept override;

        // Begin of FileSystemObject interface
	    void Copy() override;
        void Move() override;
        void Delete() override;
        // End of FileSystemObject interface

        void Draw(/*class Shader* shader*/) const;

    private:
        std::vector<Mesh> m_meshes;
        
    };
} // namespace Scald