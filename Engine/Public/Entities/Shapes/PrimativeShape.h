#pragma once

#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Components/Common/Model.h>
#include <memory>

class Model;
class TransformComponent;

class PrimativeShape : public BaseEntity
{
public:
    PrimativeShape(std::string assetRoot, std::string shapePath);
    ~PrimativeShape();

    const char* GetTypeName() const override { return "Primative Shape"; }
    Model* GetModel() { return m_BaseModel; }

private:
    Model* m_BaseModel;
};
