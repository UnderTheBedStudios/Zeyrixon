#include <Engine/Public/Entities/Common/Camera.h>
#include <string>

Camera::Camera(std::string assetRoot)
{
    std::string g_AssetRoot = assetRoot;

    m_BaseModel = std::make_unique<Model>();

    std::string cameraModelPath = g_AssetRoot + "/Engine/Models/Camera/Camera.obj";
    if (!m_BaseModel->LoadFromFile(cameraModelPath))
        fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", cameraModelPath.c_str());
}

Camera::~Camera()
{
}