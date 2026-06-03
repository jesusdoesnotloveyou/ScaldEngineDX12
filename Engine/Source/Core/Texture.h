#pragma once

#include "FileSystemObject.h"

class Texture final : public FileSystemObject
{
public:
    virtual ~Texture() override = default;

    virtual void Copy() override;
    virtual void Move() override;
    virtual void Delete() override;


};