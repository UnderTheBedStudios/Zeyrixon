#include <Engine/Public/Entities/Common/Camera.h>
#include <string>

Camera::Camera(std::string assetRoot)
{
	fov = 90.f;

	// No separate TransformComponent here — BaseEntity's constructor already added one,
	// reachable via the inherited GetTransform(). Adding a second one here was the bug:
	// every Camera ended up with two Transforms, one of them orphaned and never used.
	Model* model = AddComponent<Model>();

	const std::string cameraModelPath = assetRoot + "/Engine/Models/Camera/Camera.obj";
	if (!model->LoadFromFile(cameraModelPath))
		fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", cameraModelPath.c_str());
}

Camera::~Camera() = default;

glm::mat4 Camera::GetViewMatrix() const
{
	return glm::lookAt(GetTransform()->GetPosition(), GetTransform()->GetPosition() + (GetTransform()->GetForward() * -1.f), GetTransform()->GetUp());
}
