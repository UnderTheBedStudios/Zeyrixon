#pragma once

#include "Engine/Public/Base.h"
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <Engine/Public/Components/Common/Model.h>
#include <glm.hpp>
#include <string>

class TransformComponent; // forward-declared — full type not needed here
class Model;

class BaseEntity : public Base
{
public:
    BaseEntity();
    virtual ~BaseEntity();

    std::string Name;

    [[nodiscard]] std::string GetName() const { return Name; }
    [[nodiscard]] virtual const char* GetTypeName() const { return "Entity"; }
    [[nodiscard]] TransformComponent* GetTransform() const { return m_Transform; }
    virtual Model* GetModel() { return nullptr; }

private:
    TransformComponent* m_Transform;
};