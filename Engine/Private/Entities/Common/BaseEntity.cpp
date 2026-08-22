#include <Engine/Public/Entities/Common/BaseEntity.h>

BaseEntity::BaseEntity()
{
	// Transform only — Physics, Model, etc. are opt-in via AddComponent<T>(), driven by the
	// Editor's Add Component flow, not baked into every entity regardless of whether it
	// makes sense there (a plain empty entity doesn't need a rigid body by default).
	m_Transform = AddComponent<TransformComponent>();
}

bool BaseEntity::RemoveComponent(BaseComponent* component)
{
	for (auto it = m_Components.begin(); it != m_Components.end(); ++it)
	{
		if (it->get() == component)
		{
			m_Components.erase(it);
			return true;
		}
	}
	return false;
}
