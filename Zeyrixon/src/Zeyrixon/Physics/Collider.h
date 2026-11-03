#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/* Collider maths shared by everything that has to agree about "where is the collider and how big is it":
   the physics engine (which builds its shape from this), the viewport wireframe (which draws it) and the
   editor's resize handles (which invert it). Keeping ONE implementation is what guarantees that the green
   wireframe you see is exactly the shape Bullet simulates. */
namespace Zeyrixon::Physics
{
    /* A collider after the entity's transform has been applied. Everything is in world space, and the
       scale is already baked into the sizes. */
    struct ColliderShapeWorld
    {
        glm::vec3 Center{ 0.0f };       // where the collider is (entity position + rotated, scaled Offset)
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };   // the entity's rotation; the collider has none of its own
        glm::vec3 HalfExtents{ 0.5f };  // Box: half the size along each of its local axes
        float Radius = 0.5f;            // Sphere and Capsule
        float HalfHeight = 1.0f;        // Capsule: centre to the very tip (includes the cap), always >= Radius
    };

    /* The engine stores rotation as Euler angles (radians) that compose as Rz * Ry * Rx (see
       TransformComponent::GetMatrix). These two convert to and from a quaternion using that same order. */
    Z_API glm::quat EulerToQuat(const glm::vec3& radians);
    Z_API glm::vec3 QuatToEuler(const glm::quat& rotation);

    // One slot of a collider, after the entity's transform has been applied.
    Z_API ColliderShapeWorld ComputeWorldShape(const ColliderSlot& slot, const TransformComponent& transform);

    /* Where the whole collider balances, as an offset from the entity's origin in the entity's own space
       (scale applied, rotation not). One slot: that slot's Offset. Several: the average of their centres
       weighted by volume, so a big box with a small sphere on a stalk balances near the box instead of at the
       entity's origin. The physics body is placed there, which is what makes a dynamic body with offset
       shapes tip and roll the way you'd expect. */
    Z_API glm::vec3 ComputeCenterOfMassOffset(const ColliderComponent& collider, const TransformComponent& transform);

    // The wireframe of one slot, queued on the DebugRenderer. The caller flushes.
    Z_API void DrawColliderWireframe(const ColliderSlot& slot, const TransformComponent& transform,
                                     const glm::vec4& color);
}
