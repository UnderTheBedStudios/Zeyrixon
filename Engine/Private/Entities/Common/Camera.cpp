#include "Engine/Public/Entities/Common/Camera.h"
#include <string>

Camera::Camera(std::string assetRoot)
{
    std::string g_AssetRoot = assetRoot;

    m_Transform = new TransformComponent();

    m_Transform->SetPosition(glm::vec3(0.f, 0.f, 0.f));
    m_Transform->SetRotation(glm::vec3(0.f, 0.f, 0.f));
    m_Transform->SetScale(glm::vec3(1.f, 1.f, 1.f));

    m_BaseModel = std::make_unique<Model>();

    std::string cameraModelPath = g_AssetRoot + "/Engine/Models/Camera/Camera.obj";
    if (!m_BaseModel->LoadFromFile(cameraModelPath))
        fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", cameraModelPath.c_str());
}

Camera::~Camera()
{

}