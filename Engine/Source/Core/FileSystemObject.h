#pragma once

#include <filesystem>

using Path = std::filesystem::path;

class FileSystemObject
{
public:
    FileSystemObject() = default;
    FileSystemObject(const Path& relativePath);
    virtual ~FileSystemObject() = default;

    virtual void Copy() = 0;
    virtual void Move() = 0;
    virtual void Delete() = 0;

    Path GetPath() const;

protected:
    Path m_path;
};