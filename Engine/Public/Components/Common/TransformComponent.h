#pragma once

#include <Engine/Public/Components/Common/BaseComponent.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

struct Transform
{
	glm::vec3 Position;
	glm::quat Rotation;
	glm::vec3 Scale;
};

class TransformComponent : public BaseComponent
{
public:
	REFLECTABLE(TransformComponent, "Transform")

	TransformComponent();
	~TransformComponent();

	void SetPosition(glm::vec3 newPos);
	void SetRotation(glm::vec3 newRot);
	void SetQuaternion(glm::quat newQuat);
	void SetScale(glm::vec3);

	glm::vec3 GetPosition();
	glm::vec3 GetRotation();
	glm::quat GetQuaternion();
	glm::vec3 GetScale();

	glm::vec3 GetForward();
	glm::vec3 GetRight();
	glm::vec3 GetUp();

	glm::mat4 GetModelMatrix();

private:
	Transform m_ComponentTransform;
};
