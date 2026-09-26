#pragma once

#include <Zeyrixon/Core/Core.h>
#include <memory>
#include <string>
#include <vector>

namespace Zeyrixon
{
    class Entity;

    class Z_API World
    {
    public:
        static std::shared_ptr<World> New(const std::string& name = "Untitled");
        static std::shared_ptr<World> Load(const std::string& worldPath);

        bool Save();                          // save to m_WorldPath
        bool SaveAs(const std::string& path); // save + update m_WorldPath

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);

        const std::string& GetName() const { return m_Name; }
        const std::string& GetWorldPath() const { return m_WorldPath; }

    private:
        World() = default;

        std::string m_Name;
        std::string m_WorldPath;
        // your existing entity/component storage goes here
        // e.g. entt::registry, or the custom generic component array
    };
}