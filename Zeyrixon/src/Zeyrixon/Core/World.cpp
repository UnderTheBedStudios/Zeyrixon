#include <Zeyrixon/Core/World.h>
#include <Zeyrixon/Objects/Entity.h>
#include <pugixml.hpp>

namespace Zeyrixon
{
    std::shared_ptr<World> World::New(const std::string& name)
    {
        auto world = std::shared_ptr<World>(new World());
        world->m_Name = name;
        return world;
    }

    std::shared_ptr<World> World::Load(const std::string& worldPath)
    {
        return nullptr;
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
        return Entity{};
    }

    void World::DestroyEntity(Entity entity)
    {
        
    }
}