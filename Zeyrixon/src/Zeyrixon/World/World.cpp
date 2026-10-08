#include <pch.h>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Core/Log.h>

#include <pugixml.hpp>
#include <glm/glm.hpp>

#include <random>

namespace
{
    uint64_t GenerateEntityID()
    {
        static std::mt19937_64 engine{ std::random_device{}() };
        return engine();
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

        // TODO: Stuff to do in the future with entities

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