#pragma once

#include "FileSystemObject.h"

class Material final : public FileSystemObject
{
public:
    virtual ~Material() override = default;

    virtual void Copy() override;
    virtual void Move() override;
    virtual void Delete() override;
};