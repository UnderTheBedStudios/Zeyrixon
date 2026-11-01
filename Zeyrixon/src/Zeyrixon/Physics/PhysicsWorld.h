#pragma once

#include <Zeyrixon/Core/Core.h>

#include <entt.hpp>
#include <glm/glm.hpp>

#include <memory>

namespace Zeyrixon
{
    /* The Bullet simulation behind one World. It exists only while the world is playing: World creates it
       in OnRuntimeStart and destroys it in OnRuntimeStop, so editing never touches Bullet.

       Bullet's types are hidden behind `Impl` on purpose. Nothing that includes this header (the editor, a
       game's main.cpp) needs Bullet's include path or has to link against it directly.

       The relationship between entities and bodies:
         - an entity with a Collider becomes a body: Static if it has no RigidBody (or a Static one),
           Dynamic or Kinematic as the RigidBody says
         - Dynamic bodies are moved by the simulation, and the result is written to the entity's Transform
         - everything else is moved by YOU (scripts, animation): if an entity's Transform changed since the
           last step, the body is moved to match
       Entities that gain a Collider while playing are picked up on the next Step; entities that lose one
       (or are destroyed) must be reported through RemoveBody. */
    class Z_API PhysicsWorld
    {
    public:
        explicit PhysicsWorld(const glm::vec3& gravity);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Creates bodies for every Transform + Collider entity that doesn't have one yet.
        void AdoptNewBodies(entt::registry& registry);

        // Moves the simulation forward by dt seconds and writes the results back into the registry.
        void Step(entt::registry& registry, float dt);

        // Takes the entity's body out of the simulation. Safe to call for an entity that has none.
        void RemoveBody(entt::entity entity);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
