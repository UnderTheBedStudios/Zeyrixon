#pragma once

#include "Engine/Public/Base.h"
#include <string>

class TransformComponent; // forward-declared — full type not needed here
class Model;

class BaseEntity : public Base
{
public:
    BaseEntity();
    virtual ~BaseEntity();

    std::string Name;

    virtual const char* GetTypeName() const { return "Entity"; }
    virtual TransformComponent* GetTransform() { return nullptr; }
    virtual Model* GetModel() { return nullptr; }
};