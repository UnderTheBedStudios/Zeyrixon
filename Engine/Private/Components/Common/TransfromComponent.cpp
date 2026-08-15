#include "Engine/Public/Components/Common/TransformComponent.h"
#include <cmath>

TransformComponent::TransformComponent()
{
    ComponentTransform.Position = glm::vec3(0.f);
    ComponentTransform.Rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    ComponentTransform.Scale = glm::vec3(1.f);
}

TransformComponent::~TransformComponent()
{  }

// -------------------------------- Setting Transforms --------------------------------

void TransformComponent::SetPosition(glm::vec3 newPos) { ComponentTransform.Position = newPos; }
void TransformComponent::SetRotation(glm::vec3 newRot) { ComponentTransform.Rotation = glm::quat(glm::radians(newRot)); }
void TransformComponent::SetQuaternion(glm::quat newQuat) { ComponentTransform.Rotation = newQuat; }
void TransformComponent::SetScale(glm::vec3 newScale) { ComponentTransform.Scale = newScale; }

// -------------------------------- Getting Transforms --------------------------------

glm::vec3 TransformComponent::GetPosition() { return ComponentTransform.Position; }
glm::vec3 TransformComponent::GetRotation() { return glm::degrees(glm::eulerAngles(ComponentTransform.Rotation)); }
glm::quat TransformComponent::GetQuaternion() { return ComponentTransform.Rotation; }
glm::vec3 TransformComponent::GetScale() { return ComponentTransform.Scale; }

// -------------------------------- Getting directions --------------------------------

glm::vec3 TransformComponent::GetForward() { return glm::normalize(GetQuaternion() * glm::vec3(0.0f, 0.0f, -1.0f)); }

glm::vec3 TransformComponent::GetRight() { return glm::normalize(GetQuaternion() * glm::vec3(1.0f, 0.0f, 0.0f));}

glm::vec3 TransformComponent::GetUp() { return glm::normalize(GetQuaternion() * glm::vec3(0.0f, 1.0f, 0.0f));}