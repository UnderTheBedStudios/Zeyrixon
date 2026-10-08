#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/World.h>

#include <entt.hpp>

#include <utility>

namespace Zeyrixon
{
    class Z_API Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, World* world)
            : m_Handle(handle), m_World(world) {}

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            return m_World->GetRegistry().emplace<T>(m_Handle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            return m_World->GetRegistry().get<T>(m_Handle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_World->GetRegistry().all_of<T>(m_Handle);
        }

        template<typename T>
        void RemoveComponent()
        {
            m_World->GetRegistry().remove<T>(m_Handle);
        }

        entt::entity Handle() const { return m_Handle; }

        explicit operator bool() const
        {
            return m_World && m_World->GetRegistry().valid(m_Handle);
        }

        bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_World == other.m_World; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::entity m_Handle = entt::null;
        World* m_World = nullptr;
    };
}