#include "Model.h"
#include "Mesh.h"

using namespace Scald;

void Model::Copy() {}

void Model::Move() {}

void Model::Delete() {}

void Model::Draw() const
{
    for (const auto& mesh : m_meshes)
    {
        //mesh.Draw(/*shader*/);
    }
}