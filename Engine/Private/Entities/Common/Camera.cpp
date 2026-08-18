#include <Engine/Public/Entities/Common/Camera.h>
#include <string>

Camera::Camera(std::string assetRoot)
{
    fov = 90.f;

    m_BaseModel = std::make_unique<Model>();

    const std::string cameraModelPath = assetRoot + "/Engine/Models/Camera/Camera.obj";
    if (!m_BaseModel->LoadFromFile(cameraModelPath))
        fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", cameraModelPath.c_str());
}

Camera::~Camera() = default;

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(GetTransform()->GetPosition(), GetTransform()->GetPosition() + (GetTransform()->GetForward() * -1.f), GetTransform()->GetUp());
}
