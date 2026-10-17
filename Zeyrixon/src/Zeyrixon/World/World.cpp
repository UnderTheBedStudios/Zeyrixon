#include <pch.h>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>

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

    }

    World::~World()
    {
        // Scripts hold handles into this world, so they must be gone before it is.
        OnRuntimeStop();
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

        m_Running = true;
        return true;
    }

    void World::OnRuntimeStop()
    {
        if (!m_Running)
            return;

        ScriptEngine::OnRuntimeStop(*this);

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
        m_Running = false;
    }

    void World::OnUpdate(DeltaTime dt)
    {
        if (m_Running)
            ScriptEngine::OnRuntimeUpdate(*this, dt);
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
        world->m_WorldPath = worldPath;

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
