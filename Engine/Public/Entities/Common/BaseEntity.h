#pragma once

#include "Engine/Public/Base.h"
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <Engine/Public/Components/Common/Model.h>
#include <glm.hpp>
#include <memory>
#include <string>

class TransformComponent; // forward-declared — full type not needed here
class Model;
class PhysicsComponent; // forward-declared only — kept out of this header so Bullet's own
// headers don't propagate into every TU that includes BaseEntity.h.
// SetPhysics/ClearPhysics are defined in the .cpp, where the real
// PhysicsComponent.h is included, since unique_ptr's deleter needs
// the complete type at the point it's actually invoked.

class BaseEntity : public Base
{
public:
	BaseEntity();
	virtual ~BaseEntity();

	std::string Name;

	[[nodiscard]] std::string GetName() const { return Name; }
	[[nodiscard]] virtual const char* GetTypeName() const { return "Entity"; }
	[[nodiscard]] TransformComponent* GetTransform() const { return m_Transform; }
	virtual Model* GetModel() { return nullptr; }

	// Physics is a generic, optional slot living on BaseEntity itself — unlike Model, which
	// each shape subclass owns and exposes via its own override, any entity type can carry a
	// rigid body independent of whether it also has a visual mesh (e.g. an invisible trigger
	// volume, or a Camera you want to physically collide).
	[[nodiscard]] PhysicsComponent* GetPhysics() const { return m_Physics.get(); }
	void SetPhysics(std::unique_ptr<PhysicsComponent> physics);
	void ClearPhysics();

private:
	TransformComponent* m_Transform;
	std::unique_ptr<PhysicsComponent> m_Physics;
};
