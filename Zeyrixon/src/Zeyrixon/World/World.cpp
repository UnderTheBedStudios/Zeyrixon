#include <pch.h>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/Physics/PhysicsWorld.h>

#include <cmath>

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
    World::World()
    {
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
        for (auto [handle, id, transform] : m_Registry.view<IDComponent, TransformComponent>().each())
            m_RuntimeSnapshot[id.ID] = { transform.Position, transform.Rotation, transform.Scale };

        if (!ScriptEngine::OnRuntimeStart(*this))
        {
            m_RuntimeSnapshot.clear();
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

        // Physics starts AFTER the scripts' OnCreate, so a script that places things in OnCreate gets the
        // bodies created where it put them.
        for (auto [handle, body] : m_Registry.view<RigidBodyComponent>().each())
        {
            if (!m_Registry.all_of<ColliderComponent>(handle))
                Z_CORE_WARN("Entity '{0}' has a RigidBody but no Collider, so it has no shape and won't take part in physics.",
                            m_Registry.all_of<TagComponent>(handle) ? m_Registry.get<TagComponent>(handle).Tag : std::string("?"));
        }

        m_Physics = std::make_unique<PhysicsWorld>(m_Gravity);
        m_Physics->AdoptNewBodies(m_Registry);

        m_Running = true;
        return true;
    }

    void World::OnRuntimeStop()
    {
        if (!m_Running)
            return;

        ScriptEngine::OnRuntimeStop(*this);

        // Bodies are removed from Bullet before the transforms are restored below, so nothing can push a
        // restored transform back into a body that is about to be thrown away.
        m_Physics.reset();

        // Entities created while playing are not in the snapshot and are left as they are.
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

        UpdateAnimations(dt);
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

    void World::DestroyEntity(Entity entity)
    {
        if (!entity)
            return;

        m_Registry.destroy(entity.Handle());
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
