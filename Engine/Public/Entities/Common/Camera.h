#pragma once

#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Components/Common/Model.h>
#include <memory>

class Camera : public BaseEntity
{
public:
    Camera(std::string assetRoot);
    ~Camera();

    glm::mat4 GetViewMatrix() const;

    [[nodiscard]] const char* GetTypeName() const override { return "Camera"; }
    Model* GetModel() override { return m_BaseModel.get(); }

    float fov;

private:
    std::unique_ptr<Model> m_BaseModel;
};