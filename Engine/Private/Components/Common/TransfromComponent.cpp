#include "Engine/Public/Components/Common/TransformComponent.h"
#include <cmath>

TransformComponent::TransformComponent()
{
    m_ComponentTransform.Position = glm::vec3(0.f);
    m_ComponentTransform.Rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    m_ComponentTransform.Scale = glm::vec3(1.f);
}

TransformComponent::~TransformComponent()
{  }

// -------------------------------- Setting Transforms --------------------------------

void TransformComponent::SetPosition(glm::vec3 newPos) { m_ComponentTransform.Position = newPos; }
void TransformComponent::SetRotation(glm::vec3 newRot) { m_ComponentTransform.Rotation = glm::quat(glm::radians(newRot)); }
void TransformComponent::SetQuaternion(glm::quat newQuat) { m_ComponentTransform.Rotation = newQuat; }
void TransformComponent::SetScale(glm::vec3 newScale) { m_ComponentTransform.Scale = newScale; }

// -------------------------------- Getting Transforms --------------------------------

glm::vec3 TransformComponent::GetPosition() { return m_ComponentTransform.Position; }
glm::vec3 TransformComponent::GetRotation() { return glm::degrees(glm::eulerAngles(m_ComponentTransform.Rotation)); }
glm::quat TransformComponent::GetQuaternion() { return m_ComponentTransform.Rotation; }
glm::vec3 TransformComponent::GetScale() { return m_ComponentTransform.Scale; }

// -------------------------------- Getting directions --------------------------------

glm::vec3 TransformComponent::GetForward() { return glm::normalize(GetQuaternion() * glm::vec3(0.0f, 0.0f, -1.0f)); }

glm::vec3 TransformComponent::GetRight() { return glm::normalize(GetQuaternion() * glm::vec3(1.0f, 0.0f, 0.0f));}

glm::vec3 TransformComponent::GetUp() { return glm::normalize(GetQuaternion() * glm::vec3(0.0f, 1.0f, 0.0f));}

glm::mat4 TransformComponent::GetModelMatrix()
{
    glm::mat4 t = glm::translate(glm::mat4(1.0f), m_ComponentTransform.Position);
    glm::mat4 r = glm::mat4_cast(m_ComponentTransform.Rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), m_ComponentTransform.Scale);
    return t * r * s;
}