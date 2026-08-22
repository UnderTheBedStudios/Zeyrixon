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
	// Reuses BaseEntity's generic component array rather than owning its own Model pointer —
	// AddComponent<Model>() in the constructor puts it there.
	Model* GetModel() const { return GetComponent<Model>(); }

	float fov;
};
