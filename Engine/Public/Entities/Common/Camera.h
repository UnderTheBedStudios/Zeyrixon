#pragma once

#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <Engine/Public/Components/Common/Model.h>
#include <memory>

class Camera : public BaseEntity
{
public:
    Camera(std::string assetRoot);
    ~Camera();

    const char* GetTypeName() const override { return "Camera"; }
    TransformComponent* GetTransform() override { return m_Transform; }
    Model* GetModel() override { return m_BaseModel.get(); }

private:
    TransformComponent* m_Transform;

    std::unique_ptr<Model> m_BaseModel;
};