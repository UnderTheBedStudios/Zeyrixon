#pragma once

#include <Engine/Public/Components/Common/BaseComponent.h>
#include <Engine/Public/Components/Common/TransformComponent.h>
#include <glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <memory>

class PhysicsComponent : public BaseComponent
{
public:
	enum class ShapeType { Box, Sphere, Capsule, ConvexHall, StaticPlane };

	void Init(ShapeType shape, float mass, glm::vec3 halfExtentsOrRadius);
	void SyncTransformToPhysics(const Transform& transform);
	void SyncPhysicsToTransform(TransformComponent* transform);
	btRigidBody* GetRigidBody() { return m_Body.get(); }

	// mass == 0 is Bullet's convention for a static/immovable body. Changing mass on an
	// existing body requires recomputing local inertia and pushing it back into Bullet —
	// just writing a new value isn't enough, hence real methods here rather than a setter
	// that only touches a stored float.
	[[nodiscard]] float GetMass() const;
	void SetMass(float mass);

private:
	std::unique_ptr<btCollisionShape> m_Shape;
	std::unique_ptr<btMotionState> m_MotionState;
	std::unique_ptr<btRigidBody> m_Body;
};
