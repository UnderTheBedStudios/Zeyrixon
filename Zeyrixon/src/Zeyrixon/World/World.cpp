#include <pch.h>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/Physics/PhysicsWorld.h>
#include <Zeyrixon/Audio/AudioSystem.h>

#include <cmath>
#include <algorithm>

#include <pugixml.hpp>
#include <glm/glm.hpp>

#include <cstdio>
#include <random>

namespace
{
    uint64_t GenerateEntityID()
    {
        static std::mt19937_64 engine{ std::random_device{}() };
        return engine();
    }

    std::string Vec3ToString(const glm::vec3& v)
    {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "%.9g %.9g %.9g", v.x, v.y, v.z);
        return buffer;
    }

    glm::vec3 StringToVec3(const char* text, const glm::vec3& fallback)
    {
        glm::vec3 v;
        if (std::sscanf(text, "%f %f %f", &v.x, &v.y, &v.z) != 3)
            return fallback;
        return v;
    }
}

namespace Zeyrixon
{
    namespace { constexpr int k_MaxHierarchyDepth = 256; }   // guards every walk up the tree against a loop in a hand-edited file

    World::World()
    {
        m_Registry.on_construct<IDComponent>().connect<&World::OnIDConstructed>(this);
        m_Registry.on_destroy<IDComponent>().connect<&World::OnIDDestroyed>(this);

        /* Bodies hold no pointers into the registry, but the PhysicsWorld still has to be told when a
           collider goes away (entity deleted, or the component removed), or Bullet keeps simulating a ghost.
           EnTT calls this just BEFORE the component is destroyed. */
        m_Registry.on_destroy<ColliderComponent>().connect<&World::OnColliderDestroyed>(this);
    }

    World::~World()
    {
        // Scripts hold handles into this world, so they must be gone before it is.
        OnRuntimeStop();

        // The registry is destroyed after this body runs; make sure nothing calls back into a half-dead World.
        m_Registry.on_destroy<ColliderComponent>().disconnect<&World::OnColliderDestroyed>(this);
        m_Registry.on_construct<IDComponent>().disconnect<&World::OnIDConstructed>(this);
        m_Registry.on_destroy<IDComponent>().disconnect<&World::OnIDDestroyed>(this);
    }

    void World::OnIDConstructed(entt::registry& registry, entt::entity entity)
    {
        m_IDLookup[registry.get<IDComponent>(entity).ID] = entity;
    }

    void World::OnIDDestroyed(entt::registry& registry, entt::entity entity)
    {
        auto it = m_IDLookup.find(registry.get<IDComponent>(entity).ID);
        if (it != m_IDLookup.end() && it->second == entity)
            m_IDLookup.erase(it);
    }

    bool World::MoveAndSlide(entt::entity entity, const glm::vec3& delta)
    {
        return m_Physics && m_Physics->MoveAndSlide(m_Registry, entity, delta);
    }

    RaycastHit World::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const
    {
        return m_Physics ? m_Physics->Raycast(origin, direction, maxDistance) : RaycastHit{};
    }

    const std::vector<CollisionEvent>& World::GetCollisionEvents() const
    {
        static const std::vector<CollisionEvent> empty;
        return m_Physics ? m_Physics->GetCollisionEvents() : empty;
    }

    void World::OnColliderDestroyed(entt::registry&, entt::entity entity)
    {
        if (m_Physics)
            m_Physics->RemoveBody(entity);
    }

    bool World::OnRuntimeStart()
    {
        if (m_Running)
            return true;

        m_RuntimeSnapshot.clear();
        m_RuntimeEntityIDs.clear();
        for (auto [handle, id] : m_Registry.view<IDComponent>().each())
            m_RuntimeEntityIDs.insert(id.ID);
        for (auto [handle, id, transform] : m_Registry.view<IDComponent, TransformComponent>().each())
            m_RuntimeSnapshot[id.ID] = { transform.Position, transform.Rotation, transform.Scale };

        // The simulation exists BEFORE the scripts' OnCreate so a script can already call Entity.Move there.
        // It starts empty: bodies are created further down, once OnCreate has had its say about positions.
        m_Physics = std::make_unique<PhysicsWorld>(m_Gravity);
        m_Audio = std::make_unique<AudioSystem>(*this);

        if (!ScriptEngine::OnRuntimeStart(*this))
        {
            m_Physics.reset();
            m_Audio.reset();
            m_RuntimeSnapshot.clear();
            m_RuntimeEntityIDs.clear();
            return false;
        }

        // Animations start from the top (when they're set to play on start); the editor's time is put back on Stop.
        m_AnimationTimeSnapshot.clear();
        for (auto [handle, id, animation] : m_Registry.view<IDComponent, AnimationComponent>().each())
        {
            m_AnimationTimeSnapshot[id.ID] = animation.Time;
            animation.Time = 0.0f;
            animation.Playing = animation.PlayOnStart;
        }

        // Bodies are created AFTER the scripts' OnCreate, so a script that places things in OnCreate gets the
        // bodies created where it put them.
        for (auto [handle, body] : m_Registry.view<RigidBodyComponent>().each())
        {
            if (!m_Registry.all_of<ColliderComponent>(handle))
                Z_CORE_WARN("Entity '{0}' has a RigidBody but no Collider, so it has no shape and won't take part in physics.",
                            m_Registry.all_of<TagComponent>(handle) ? m_Registry.get<TagComponent>(handle).Tag : std::string("?"));
        }

        // Physics still reads and writes an entity's transform as if it were a world transform, so a
        // collider on a CHILD would be built in the wrong place. Until that is taught about parents, say so.
        for (auto [handle, id, collider] : m_Registry.view<IDComponent, ColliderComponent>().each())
        {
            if (id.ParentID != 0)
                Z_CORE_WARN("Entity '{0}' has a Collider and a parent. Physics doesn't understand parents yet, so keep colliders and rigid bodies on root entities.",
                            m_Registry.all_of<TagComponent>(handle) ? m_Registry.get<TagComponent>(handle).Tag : std::string("?"));
        }

        m_Physics->AdoptNewBodies(m_Registry);

        m_Running = true;
        return true;
    }

    void World::OnRuntimeStop()
    {
        if (!m_Running)
            return;

        ScriptEngine::OnRuntimeStop(*this);
        m_Audio.reset();

        // Remove entities created during Play, starting with the new roots in each hierarchy.
        // DestroyEntity removes the full subtree and updates the original parent.
        std::vector<uint64_t> runtimeRoots;
        for (auto [handle, id] : m_Registry.view<IDComponent>().each())
        {
            if (m_RuntimeEntityIDs.count(id.ID))
                continue;
            if (id.ParentID == 0 || m_RuntimeEntityIDs.count(id.ParentID))
                runtimeRoots.push_back(id.ID);
        }
        for (uint64_t id : runtimeRoots)
            DestroyEntity(FindEntityByID(id));

        // Bodies are removed from Bullet before the transforms are restored below, so nothing can push a
        // restored transform back into a body that is about to be thrown away.
        m_Physics.reset();

        for (auto [handle, id, transform] : m_Registry.view<IDComponent, TransformComponent>().each())
        {
            auto saved = m_RuntimeSnapshot.find(id.ID);
            if (saved == m_RuntimeSnapshot.end())
                continue;

            transform.Position = saved->second.Position;
            transform.Rotation = saved->second.Rotation;
            transform.Scale = saved->second.Scale;
        }

        m_RuntimeSnapshot.clear();
        m_RuntimeEntityIDs.clear();

        for (auto [handle, id, animation] : m_Registry.view<IDComponent, AnimationComponent>().each())
        {
            animation.Playing = false;
            auto saved = m_AnimationTimeSnapshot.find(id.ID);
            animation.Time = saved != m_AnimationTimeSnapshot.end() ? saved->second : 0.0f;
        }
        m_AnimationTimeSnapshot.clear();

        m_Running = false;
    }

    void World::OnUpdate(DeltaTime dt)
    {
        if (!m_Running)
            return;

        ScriptEngine::OnRuntimeUpdate(*this, dt);

        // After the scripts, so a transform a script just changed is what the simulation starts from.
        if (m_Physics)
            m_Physics->Step(m_Registry, dt.GetSeconds());

        // Deliver contacts after Bullet has completed the step, so callbacks observe current transforms.
        ScriptEngine::OnCollisionEvents(*this);

        UpdateAnimations(dt);
        if (m_Audio)
            m_Audio->Update();
    }

    bool World::PlayAudio(uint64_t entityID, bool restart)
    {
        return m_Audio && m_Audio->Play(entityID, restart);
    }

    void World::StopAudio(uint64_t entityID)
    {
        if (m_Audio)
            m_Audio->Stop(entityID);
    }

    bool World::PlayAudioFile(const std::string& path, float volume, float pitch)
    {
        return m_Audio && m_Audio->PlayOneShot(path, volume, pitch);
    }

    bool World::PlayAudioAt(const std::string& path, const glm::vec3& position, float volume, float pitch)
    {
        return m_Audio && m_Audio->PlayOneShot(path, volume, pitch, &position);
    }

    void World::SetMasterAudioVolume(float volume)
    {
        if (m_Audio)
            m_Audio->SetMasterVolume(volume);
    }

    void World::UpdateAnimations(DeltaTime dt)
    {
        for (auto [handle, mesh, animation] : m_Registry.view<MeshComponent, AnimationComponent>().each())
        {
            if (!animation.Playing)
                continue;

            std::shared_ptr<Mesh> meshData = MeshLibrary::Get(mesh.MeshPath);
            const ModelData* model = meshData ? meshData->GetModel().get() : nullptr;
            if (!model || animation.ClipIndex < 0 || animation.ClipIndex >= static_cast<int>(model->Animations.size()))
                continue;

            const float duration = model->Animations[animation.ClipIndex].Duration;
            animation.Time += dt.GetSeconds() * animation.Speed;

            if (duration <= 0.0f)
            {
                animation.Time = 0.0f;
            }
            else if (animation.Loop)
            {
                animation.Time = std::fmod(animation.Time, duration);
                if (animation.Time < 0.0f)
                    animation.Time += duration;   // fmod keeps the sign: wrap backwards playback too
            }
            else if (animation.Time >= duration)
            {
                animation.Time = duration;        // hold the last frame
                animation.Playing = false;
            }
            else if (animation.Time < 0.0f)
            {
                animation.Time = 0.0f;
                animation.Playing = false;
            }
        }
    }

    std::shared_ptr<World> World::New(const std::string& name)
    {
        auto world = std::shared_ptr<World>(new World());
        world->m_Name = name;
        return world;
    }

    std::shared_ptr<World> World::Load(const std::string& worldPath)
    {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(worldPath.c_str());
        if (!result)
        {
            Z_CORE_ERROR("Failed to load world '{0}': {1}", worldPath, result.description());
            return nullptr;
        }

        pugi::xml_node root = doc.child("World");
        if (!root)
        {
            Z_CORE_ERROR("World file '{0}' has no <World> root node", worldPath);
            return nullptr;
        }

        auto world = std::shared_ptr<World>(new World());
        world->m_Name = root.attribute("Name").as_string("Untitled");
        world->SetAmbientIntensity(root.attribute("Ambient").as_float(world->GetAmbientIntensity()));   // old files: default
        world->m_WorldPath = worldPath;
        world->m_Gravity = StringToVec3(root.attribute("Gravity").as_string(""), world->m_Gravity);   // old files: Earth

        for (pugi::xml_node node : root.children("Entity"))
        {
            uint64_t id = node.attribute("ID").as_ullong(0);
            if (id == 0)
                id = GenerateEntityID();

            Entity entity{ world->m_Registry.create(), world.get() };
            entity.AddComponent<IDComponent>(id);
            entity.GetComponent<IDComponent>().ParentID = node.attribute("Parent").as_ullong(0);   // linked up after the loop

            // Every entity always has a Tag and a Transform, same as CreateEntity.
            pugi::xml_node tagNode = node.child("Tag");
            entity.AddComponent<TagComponent>(std::string(tagNode.attribute("Value").as_string("Entity")));

            for (auto& info : ComponentRegistry::Get().GetAll())
            {
                pugi::xml_node componentNode = node.child(info.Name.c_str());

                // Core components always exist, with defaults if the file has no node.
                // Everything else exists only if the file lists it.
                if (!componentNode && !info.Core)
                    continue;

                if (!info.Has(entity))
                    info.Add(entity);

                if (info.Deserialize)
                    info.Deserialize(entity, componentNode);
            }
        }

        // Second pass: now every entity exists, hook children up to their parents. A parent that isn't in
        // the file, or one that would make a loop, is dropped (the entity becomes a root) with a warning.
        for (auto handle : world->m_Registry.view<IDComponent>())
        {
            auto& idComponent = world->m_Registry.get<IDComponent>(handle);
            if (idComponent.ParentID == 0)
                continue;

            const entt::entity parentHandle = world->FindHandleByID(idComponent.ParentID);
            const bool loop = parentHandle == handle
                || (parentHandle != entt::null && world->IsDescendantOf(Entity{ parentHandle, world.get() }, Entity{ handle, world.get() }));
            if (parentHandle == entt::null || loop)
            {
                Z_CORE_WARN("World '{0}': entity {1} has {2} parent {3}; making it a root.",
                            world->m_Name, idComponent.ID, loop ? "a looping" : "a missing", idComponent.ParentID);
                idComponent.ParentID = 0;
                continue;
            }
        }

        // Views run newest-first; walk them in reverse so each parent's ChildIDs ends up in file order.
        {
            std::vector<entt::entity> all;
            for (auto handle : world->m_Registry.view<IDComponent>())
                all.push_back(handle);
            for (auto it = all.rbegin(); it != all.rend(); ++it)
            {
                const auto& idComponent = world->m_Registry.get<IDComponent>(*it);
                if (idComponent.ParentID == 0)
                    continue;
                world->m_Registry.get<IDComponent>(world->FindHandleByID(idComponent.ParentID)).ChildIDs.push_back(idComponent.ID);
            }
        }

        return world;
    }

    bool World::SaveAs(const std::string& path)
    {
        m_WorldPath = path;
        return Save();
    }

    bool World::Save()
    {
        // While playing, the transforms are whatever the scripts made them. Saving now would
        // write that into the file, so refuse; stop the game first.
        if (m_Running)
        {
            Z_CORE_ERROR("Can't save world '{0}' while the game is running. Press Stop first.", m_Name);
            return false;
        }

        pugi::xml_document doc;
        auto root = doc.append_child("World");
        root.append_attribute("Name") = m_Name.c_str();
        root.append_attribute("Ambient") = m_AmbientIntensity;
        root.append_attribute("Gravity") = Vec3ToString(m_Gravity).c_str();

        // Views iterate newest-first, so collect the handles and write them in
        // reverse. That keeps the file in creation order.
        std::vector<entt::entity> handles;
        for (auto handle : m_Registry.view<IDComponent>())
            handles.push_back(handle);

        for (auto it = handles.rbegin(); it != handles.rend(); ++it)
        {
            Entity entity{ *it, this };

            auto node = root.append_child("Entity");
            node.append_attribute("ID") = static_cast<unsigned long long>(entity.GetComponent<IDComponent>().ID);
            if (entity.GetComponent<IDComponent>().ParentID != 0)
                node.append_attribute("Parent") = static_cast<unsigned long long>(entity.GetComponent<IDComponent>().ParentID);

            for (auto& info : ComponentRegistry::Get().GetAll())
            {
                if (!info.Serialize || !info.Has(entity))
                    continue;

                info.Serialize(entity, node.append_child(info.Name.c_str()));
            }

            if (entity.HasComponent<TransformComponent>())
            {
                auto& transform = entity.GetComponent<TransformComponent>();
                auto transformNode = node.append_child("Transform");
                transformNode.append_attribute("Position") = Vec3ToString(transform.Position).c_str();
                transformNode.append_attribute("Rotation") = Vec3ToString(transform.Rotation).c_str();
                transformNode.append_attribute("Scale")    = Vec3ToString(transform.Scale).c_str();
            }
        }

        return doc.save_file(m_WorldPath.c_str());
    }

    Entity World::CreateEntity(const std::string& name)
    {
        Entity entity{ m_Registry.create(), this };
        entity.AddComponent<IDComponent>(GenerateEntityID());

        for (auto& info : ComponentRegistry::Get().GetAll())
        {
            if (info.Core && !info.Has(entity))
                info.Add(entity);
        }

        entity.GetComponent<TagComponent>().Tag = name;
        return entity;
    }

    Entity World::CreateEntityWithID(uint64_t id, const std::string& name)
    {
        Entity entity{ m_Registry.create(), this };
        entity.AddComponent<IDComponent>(id);

        for (auto& info : ComponentRegistry::Get().GetAll())
        {
            if (info.Core && !info.Has(entity))
                info.Add(entity);
        }

        entity.GetComponent<TagComponent>().Tag = name;
        return entity;
    }

    Entity World::FindEntityByID(uint64_t id)
    {
        const entt::entity handle = FindHandleByID(id);
        return handle != entt::null ? Entity{ handle, this } : Entity{};
    }

    entt::entity World::FindHandleByID(uint64_t id) const
    {
        auto it = m_IDLookup.find(id);
        return it != m_IDLookup.end() ? it->second : entt::null;
    }

    void World::DestroyEntity(Entity entity)
    {
        if (!entity)
            return;

        // Everything beneath it goes too. Collect the handles first, then destroy children before parents.
        const uint64_t id = entity.GetComponent<IDComponent>().ID;
        std::vector<entt::entity> doomed;
        for (uint64_t subtreeID : GetSubtreeIDs(id))
        {
            const entt::entity handle = FindHandleByID(subtreeID);
            if (handle != entt::null)
                doomed.push_back(handle);
        }

        // Take the root out of its parent's child list.
        const uint64_t parentID = entity.GetComponent<IDComponent>().ParentID;
        if (parentID != 0)
        {
            const entt::entity parentHandle = FindHandleByID(parentID);
            if (parentHandle != entt::null)
            {
                auto& siblings = m_Registry.get<IDComponent>(parentHandle).ChildIDs;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
            }
        }

        for (auto it = doomed.rbegin(); it != doomed.rend(); ++it)
            m_Registry.destroy(*it);
    }

    // ---- Hierarchy ----

    bool World::IsDescendantOf(Entity entity, Entity possibleAncestor)
    {
        if (!entity || !possibleAncestor)
            return false;

        const uint64_t ancestorID = possibleAncestor.GetComponent<IDComponent>().ID;
        uint64_t current = entity.GetComponent<IDComponent>().ParentID;
        for (int hops = 0; current != 0 && hops < k_MaxHierarchyDepth; ++hops)
        {
            if (current == ancestorID)
                return true;

            const entt::entity handle = FindHandleByID(current);
            current = handle != entt::null ? m_Registry.get<IDComponent>(handle).ParentID : 0;
        }
        return false;
    }

    bool World::SetParent(Entity child, Entity parent, bool keepWorldTransform)
    {
        if (!child)
            return false;
        if (parent && (parent == child || IsDescendantOf(parent, child)))
            return false;

        auto& childID = child.GetComponent<IDComponent>();
        const uint64_t newParentID = parent ? parent.GetComponent<IDComponent>().ID : 0;
        if (childID.ParentID == newParentID)
            return true;

        const glm::mat4 world = GetWorldMatrix(child.Handle());   // where it is now, before anything moves

        // Out of the old parent's list...
        if (childID.ParentID != 0)
        {
            const entt::entity oldParent = FindHandleByID(childID.ParentID);
            if (oldParent != entt::null)
            {
                auto& siblings = m_Registry.get<IDComponent>(oldParent).ChildIDs;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), childID.ID), siblings.end());
            }
        }

        // ...into the new one's.
        childID.ParentID = newParentID;
        if (parent)
            parent.GetComponent<IDComponent>().ChildIDs.push_back(childID.ID);

        if (keepWorldTransform && child.HasComponent<TransformComponent>())
        {
            const glm::mat4 parentWorld = GetParentWorldMatrix(child.Handle());
            if (std::abs(glm::determinant(parentWorld)) > 1e-12f)   // a parent scaled to zero can't be undone
                child.GetComponent<TransformComponent>().SetFromMatrix(glm::inverse(parentWorld) * world);
        }
        return true;
    }

    Entity World::GetParent(Entity entity)
    {
        if (!entity)
            return Entity{};
        return FindEntityByID(entity.GetComponent<IDComponent>().ParentID);   // ID 0 finds nothing
    }

    std::vector<Entity> World::GetChildren(Entity entity)
    {
        std::vector<Entity> children;
        if (!entity)
            return children;

        for (uint64_t childID : entity.GetComponent<IDComponent>().ChildIDs)
        {
            Entity child = FindEntityByID(childID);
            if (child)
                children.push_back(child);
        }
        return children;
    }

    std::vector<uint64_t> World::GetSubtreeIDs(uint64_t rootID) const
    {
        std::vector<uint64_t> result;
        std::vector<uint64_t> stack{ rootID };
        const size_t limit = m_Registry.view<IDComponent>().size();   // a loop could never produce more than this

        while (!stack.empty() && result.size() <= limit)
        {
            const uint64_t id = stack.back();
            stack.pop_back();

            const entt::entity handle = FindHandleByID(id);
            if (handle == entt::null)
                continue;

            result.push_back(id);
            const auto& children = m_Registry.get<IDComponent>(handle).ChildIDs;
            for (auto it = children.rbegin(); it != children.rend(); ++it)   // reversed so the first child pops first
                stack.push_back(*it);
        }
        return result;
    }

    glm::mat4 World::GetWorldMatrix(entt::entity entity) const
    {
        glm::mat4 world(1.0f);
        for (int hops = 0; entity != entt::null && m_Registry.valid(entity) && hops < k_MaxHierarchyDepth; ++hops)
        {
            if (const auto* transform = m_Registry.try_get<TransformComponent>(entity))
                world = transform->GetMatrix() * world;   // each step up puts the parent on the left

            const auto* id = m_Registry.try_get<IDComponent>(entity);
            entity = (id && id->ParentID != 0) ? FindHandleByID(id->ParentID) : entt::null;
        }
        return world;
    }

    glm::mat4 World::GetParentWorldMatrix(entt::entity entity) const
    {
        const auto* id = m_Registry.valid(entity) ? m_Registry.try_get<IDComponent>(entity) : nullptr;
        return (id && id->ParentID != 0) ? GetWorldMatrix(FindHandleByID(id->ParentID)) : glm::mat4(1.0f);
    }

    glm::vec3 World::GetWorldPosition(entt::entity entity) const
    {
        return glm::vec3(GetWorldMatrix(entity)[3]);
    }

    TransformComponent World::GetWorldTransform(entt::entity entity) const
    {
        TransformComponent result;
        result.SetFromMatrix(GetWorldMatrix(entity));
        return result;
    }

    glm::mat4 World::GetWorldViewMatrix(entt::entity entity) const
    {
        glm::mat4 cameraToWorld = GetWorldMatrix(entity);
        for (int axis = 0; axis < 3; ++axis)   // a camera ignores scale
        {
            const float length = glm::length(glm::vec3(cameraToWorld[axis]));
            if (length > 1e-6f)
                cameraToWorld[axis] /= length;
        }
        return glm::inverse(cameraToWorld);
    }

    Entity World::GetPrimaryCamera()
    {
        auto cameras = m_Registry.view<CameraComponent, TransformComponent>();
        for (auto [handle, camera, transform] : cameras.each())
        {
            if (camera.Primary)
                return Entity{ handle, this };
        }

        return Entity{};
    }
}
