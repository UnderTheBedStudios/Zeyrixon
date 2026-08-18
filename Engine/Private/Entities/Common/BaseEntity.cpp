#include "Engine/Public/Entities/Common/BaseEntity.h"

BaseEntity::BaseEntity()
{
    m_Transform = new TransformComponent();

    m_Transform->SetPosition(glm::vec3(0.f, 0.f, 0.f));
    m_Transform->SetRotation(glm::vec3(0.f, 0.f, 0.f));
    m_Transform->SetScale(glm::vec3(1.f, 1.f, 1.f));
}
BaseEntity::~BaseEntity()
{
    delete m_Transform;
}