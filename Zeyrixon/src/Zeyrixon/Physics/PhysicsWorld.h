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

        /* Moves a Kinematic (or Static) entity by `delta`, but never into a wall.

           The collider's real shape is swept along the movement. If it would hit something solid it stops
           just short, and what is left of the movement is slid along the surface it hit, up to a few times,
           so walking into a wall at an angle slides along it instead of sticking, and a corner is handled.
           It writes the resulting position into the entity's Transform.

           What counts as solid: Static bodies only (colliders with no RigidBody, or a Static one). Dynamic
           bodies don't block the mover; the simulation pushes them out of the way, as a kinematic body
           should. Other kinematic bodies don't block it either.

           Returns true if the movement was stopped or redirected by something. A script that moves straight
           down each frame gets "am I standing on something" for free from that. Returns false (and does
           nothing) for a Dynamic body, which the simulation owns, or an entity without a Collider. */
        bool MoveAndSlide(entt::registry& registry, entt::entity entity, const glm::vec3& delta);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
