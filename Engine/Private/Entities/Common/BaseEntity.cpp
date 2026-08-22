#include "Engine/Public/Entities/Common/BaseEntity.h"
#include "Engine/Public/Components/Common/PhysicsComponent.h"

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
	// m_Physics (unique_ptr<PhysicsComponent>) cleans itself up here — PhysicsComponent.h is
	// included above specifically so this destructor has the complete type it needs.
}

void BaseEntity::SetPhysics(std::unique_ptr<PhysicsComponent> physics)
{
	m_Physics = std::move(physics);
}

void BaseEntity::ClearPhysics()
{
	m_Physics.reset();
}
