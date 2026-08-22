#include <Engine/Public/Entities/Common/Camera.h>
#include <string>

#include "Engine/Public/Components/Common/TransformComponent.h"
#include <Engine/Public/Components/Common/Model.h>

Camera::Camera(std::string assetRoot)
{
    fov = 90.f;

    Model* model = AddComponent<Model>();
	m_transform = AddComponent<TransformComponent>();

    const std::string cameraModelPath = assetRoot + "/Engine/Models/Camera/Camera.obj";
    if (!model->LoadFromFile(cameraModelPath))
        fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", cameraModelPath.c_str());
}

Camera::~Camera() = default;

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(m_transform->GetPosition(), m_transform->GetPosition() + (m_transform->GetForward() * -1.f), m_transform->GetUp());
}
