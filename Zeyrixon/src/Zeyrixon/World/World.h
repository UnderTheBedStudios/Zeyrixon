#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/DeltaTime.h>
#include <Zeyrixon/World/Components.h>

#include <entt.hpp>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zeyrixon
{
    class Entity;
    class PhysicsWorld;
    struct RaycastHit;
    struct CollisionEvent;

    class Z_API World
    {
    public:
         World();
         ~World();

         /* Play mode. OnRuntimeStart remembers every entity's transform, compiles and starts the
            scripts and returns false if a script fails to compile (the world then is not running).
            OnRuntimeStop stops the scripts and puts the transforms back, so playing never changes
            the world you were editing. OnUpdate runs the scripts, and does nothing when stopped. */
         bool OnRuntimeStart();
         void OnRuntimeStop();
         bool IsRunning() const { return m_Running; }

         void OnUpdate(DeltaTime dt);

         static std::shared_ptr<World> New(const std::string& name = "Untitled");
         static std::shared_ptr<World> Load(const std::string& worldPath);

         bool Save();                          // save to m_WorldPath
         bool SaveAs(const std::string& path); // save + update m_WorldPath

         Entity CreateEntity(const std::string& name = "Entity");
         Entity CreateEntityWithID(uint64_t id, const std::string& name = "Entity");   // used by undo/redo to bring an entity back with its old ID
         Entity FindEntityByID(uint64_t id);                                           // invalid Entity if there's none
         entt::entity FindHandleByID(uint64_t id) const;                               // entt::null if there's none
         void DestroyEntity(Entity entity);                                            // also destroys all of its children, grandchildren...

         // ---- Hierarchy ----
         // A child's TransformComponent is LOCAL: relative to its parent. The world matrix is every
         // ancestor's transform multiplied together, and that is what rendering, picking, lights and
         // cameras use.

         /* Makes `parent` the parent of `child`; an invalid Entity as parent makes the child a root again.
            With keepWorldTransform (the default) the child's local transform is rewritten so it stays exactly
            where it is on screen; without it the local transform is kept and the child jumps. Returns false
            (and changes nothing) if it would make a loop: an entity can't be its own parent or be put under
            one of its own descendants. */
         bool SetParent(Entity child, Entity parent, bool keepWorldTransform = true);
         Entity GetParent(Entity entity);
         std::vector<Entity> GetChildren(Entity entity);
         bool IsDescendantOf(Entity entity, Entity possibleAncestor);

         // The entity's ID followed by the IDs of everything beneath it, parents always before their children.
         std::vector<uint64_t> GetSubtreeIDs(uint64_t rootID) const;

         glm::mat4 GetWorldMatrix(entt::entity entity) const;
         glm::mat4 GetParentWorldMatrix(entt::entity entity) const;      // identity for a root
         glm::vec3 GetWorldPosition(entt::entity entity) const;
         TransformComponent GetWorldTransform(entt::entity entity) const; // the world matrix taken apart again
         glm::mat4 GetWorldViewMatrix(entt::entity entity) const;         // a camera's view matrix (scale ignored)

         // The first entity with a camera component marked Primary, or invalid Entity if there isn't one.
         Entity GetPrimaryCamera();

         const std::string& GetName() const { return m_Name; }
         const std::string& GetWorldPath() const { return m_WorldPath; }

         entt::registry& GetRegistry() { return m_Registry; }

         /* How strong the flat background light is, 0 = none (only real lights light things), 1 = the
            old fixed brightness. Saved with the world. */
         float GetAmbientIntensity() const { return m_AmbientIntensity; }
         void SetAmbientIntensity(float value) { m_AmbientIntensity = value < 0.0f ? 0.0f : value; }

         /* Gravity in world units per second squared; (0, -9.81, 0) is Earth. Saved with the world. It is read
            when Play starts, so changing it while playing has no effect until the next Play. */
         /* Moves an entity by `delta` without letting it pass through static colliders; see
            PhysicsWorld::MoveAndSlide for the rules. Only meaningful while playing; returns false otherwise,
            and for entities that aren't Kinematic/Static bodies. This is what a script's Entity.Move calls. */
         bool MoveAndSlide(entt::entity entity, const glm::vec3& delta);
         RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;
         const std::vector<CollisionEvent>& GetCollisionEvents() const;

         const glm::vec3& GetGravity() const { return m_Gravity; }
         void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }

      private:
         std::string m_Name;
         std::string m_WorldPath;
         float m_AmbientIntensity = 0.1f;
         glm::vec3 m_Gravity{ 0.0f, -9.81f, 0.0f };

         // ID -> entity, kept up to date by the registry's construct/destroy signals. Declared before the
         // registry so it outlives it.
         std::unordered_map<uint64_t, entt::entity> m_IDLookup;
         void OnIDConstructed(entt::registry& registry, entt::entity entity);
         void OnIDDestroyed(entt::registry& registry, entt::entity entity);

         entt::registry m_Registry;

         /* The Bullet simulation. Exists only while playing. Declared AFTER the registry so it is destroyed
            first, and World::OnColliderDestroyed (below) is how it hears about colliders going away. */
         std::unique_ptr<PhysicsWorld> m_Physics;
         void OnColliderDestroyed(entt::registry& registry, entt::entity entity);

         // Moves every playing Animation component forward by dt (looping or stopping at the end).
         void UpdateAnimations(DeltaTime dt);

         bool m_Running = false;

         struct TransformSnapshot { glm::vec3 Position, Rotation, Scale; };
         std::unordered_map<uint64_t, TransformSnapshot> m_RuntimeSnapshot;   // keyed by IDComponent::ID
         std::unordered_map<uint64_t, float> m_AnimationTimeSnapshot;         // same key: Animation time before Play
    };
}
