#pragma once

#include "Engine/Public/Entities/Common/BaseEntity.h"
#include "Engine/Public/Components/Common/TransformComponent.h"
#include "Engine/Public/Components/Common/Model.h"
#include <memory>

class Camera : public BaseEntity
{
public:
    Camera(std::string assetRoot);
    ~Camera();

private:
    TransformComponent* m_Transform;

    std::unique_ptr<Model> m_BaseModel;
};