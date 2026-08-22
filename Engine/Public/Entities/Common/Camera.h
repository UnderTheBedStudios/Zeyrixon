#pragma once

#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <memory>

#include "Engine/Public/Components/Common/TransformComponent.h"

class Camera : public BaseEntity
{
public:
    Camera(std::string assetRoot);
    ~Camera();

    glm::mat4 GetViewMatrix() const;

    [[nodiscard]] const char* GetTypeName() const override { return "Camera"; }

    float fov;
private:
	TransformComponent* m_transform;
};
