#pragma once

#include "Engine/Public/Base.h"
#include <Engine/Public/Components/Common/BaseComponent.h>
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

class BaseEntity : public Base
{
public:
	BaseEntity();
	~BaseEntity() override = default;

	std::string Name;

	[[nodiscard]] std::string GetName() const { return Name; }
	[[nodiscard]] virtual const char* GetTypeName() const { return "Entity"; }

	// Every entity gets exactly one of these, added by the constructor below. The rest of
	// the engine (rendering, physics sync, the Inspector) assumes it's always present and
	// never null, so it's kept as its own guaranteed accessor rather than something callers
	// have to fetch via GetComponent<T>() and null-check every time.
	[[nodiscard]] TransformComponent* GetTransform() const { return m_Transform; }

	// All components on this entity, type-erased — used by the Editor's generic Inspector
	// loop, which draws whatever REFLECTABLE fields each one has without needing to know
	// concrete types.
	[[nodiscard]] const std::vector<std::unique_ptr<BaseComponent>>& GetComponents() const { return m_Components; }

	// Constructs a new T (forwarding any constructor args) and adds it to this entity's
	// component array. Multiple components of the same type are allowed — e.g. more than one
	// Model for a multi-part mesh, more than one PhysicsComponent for a compound collider.
	// Only Transform is special-cased (see above); everything else, including Physics and
	// Model, is opt-in via this call — nothing is added automatically except Transform.
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<BaseComponent, T>,
					  "Error: The specified class type must inherit from BaseComponent!");
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = component.get();
		m_Components.push_back(std::move(component));
		return raw;
	}

	// First component of type T on this entity, or nullptr.
	template <typename T>
	[[nodiscard]] T* GetComponent() const
	{
		for (const std::unique_ptr<BaseComponent>& c : m_Components)
			if (T* match = dynamic_cast<T*>(c.get()))
				return match;
		return nullptr;
	}

	// Removes one specific component instance by identity. This is identity-based rather
	// than type-based on purpose: duplicates by type are allowed now, so "remove the physics
	// one" only makes sense as "remove this particular instance", not "remove whichever
	// PhysicsComponent happens to be first". Returns false if `component` isn't in this
	// entity's array (already removed, or belongs to a different entity).
	bool RemoveComponent(BaseComponent* component);

private:
	TransformComponent* m_Transform;
	std::vector<std::unique_ptr<BaseComponent>> m_Components;
};
