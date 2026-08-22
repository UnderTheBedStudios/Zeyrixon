#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <Engine/Public/Components/Common/PhysicsComponent.h>

BaseEntity::BaseEntity()
{
	AddComponent<TransformComponent>();
	AddComponent<PhysicsComponent>();
}

BaseComponent* BaseEntity::GetComponent(std::string name, std::string type)
{
	BaseComponent* g_component = nullptr;

	for (BaseComponent* component : m_Components)
	{
		if (component->GetName() == name && component->GetType() == type)
		{
			g_component = component;
			return g_component;
		}
	}

	return nullptr;
}
