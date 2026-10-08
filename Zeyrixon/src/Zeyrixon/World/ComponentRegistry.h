#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/Entity.h>

#include <pugixml.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Zeyrixon
{
    struct ComponentInfo
    {
        std::string Name;   // XML node name and inspector header
        bool Core = false;  // On every entity; never added or removed by hand

        std::function<bool(Entity)> Has;
        std::function<void(Entity)> Add;
        std::function<void(Entity)> Remove;

        /* Engine side. Deserialize recieves an empty node when a core component
           has no entry in the file, and must then leave the defaults alone. */
        std::function<void(Entity, pugi::xml_node)> Serialize;
        std::function<void(Entity, pugi::xml_node)> Deserialize;

        // Editor side. Left empty by the engine; the editor assigns it.
        std::function<void(Entity)> DrawInspector;
    };

    class Z_API ComponentRegistry
    {
    public:
        static ComponentRegistry& Get();

        /* Returns the new entry so the caller can fill in Serialize/Deserialize.
           Don't hold the reference across another Register call. */
        template<typename T>
        ComponentInfo& Register(const std::string& name, bool core = false)
        {
            ComponentInfo info;
            info.Name = name;
            info.Core = core;
            info.Has    = [](Entity e) { return e.HasComponent<T>(); };
            info.Add    = [](Entity e) { e.AddComponent<T>(); };
            info.Remove = [](Entity e) { e.RemoveComponent<T>(); };
            m_Components.push_back(std::move(info));
            return m_Components.back();
        }

        ComponentInfo* Find(const std::string& name);
        std::vector<ComponentInfo>& GetAll() { return m_Components; }
        
    private:
        ComponentRegistry();
        std::vector<ComponentInfo> m_Components;
    };
}