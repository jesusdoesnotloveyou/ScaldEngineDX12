#include "stdafx.h"
#include "FileSystem.h"

FileSystemObject::FileSystemObject(const Path& relativePath)
    : m_path(relativePath)
{
    assert(relativePath.is_relative());
}

Path FileSystemObject::GetPath() const
{
    return m_path;
}