#pragma once

#include "FileSystemObject.h"

class Model final : public FileSystemObject
{
public:

	virtual ~Model() override = default;

	virtual void Copy() override;
    virtual void Move() override;
    virtual void Delete() override;
};