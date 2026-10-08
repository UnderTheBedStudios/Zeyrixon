#include <pch.h>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Core/Log.h>

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
    {}

    void World::OnUpdate(DeltaTime dt)
    {

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

            pugi::xml_node transformNode = node.child("Transform");
            auto& transform = entity.AddComponent<TransformComponent>();
            transform.Position = StringToVec3(transformNode.attribute("Position").as_string(""), transform.Position);
            transform.Rotation = StringToVec3(transformNode.attribute("Rotation").as_string(""), transform.Rotation);
            transform.Scale    = StringToVec3(transformNode.attribute("Scale").as_string(""),    transform.Scale);
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

            if (entity.HasComponent<TagComponent>())
                node.append_child("Tag").append_attribute("Value") = entity.GetComponent<TagComponent>().Tag.c_str();

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
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();
        return entity;
    }

    void World::DestroyEntity(Entity entity)
    {
        if (!entity)
            return;

        m_Registry.destroy(entity.Handle());
    }
}