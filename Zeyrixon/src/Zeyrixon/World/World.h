#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/DeltaTime.h>

#include <entt.hpp>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zeyrixon
{
    class Entity;

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
        void DestroyEntity(Entity entity);

        // The first entity with a camera component marked Primary, or invalid Entity if there isn't one.
        Entity GetPrimaryCamera();

        const std::string& GetName() const { return m_Name; }
        const std::string& GetWorldPath() const { return m_WorldPath; }

        entt::registry& GetRegistry() { return m_Registry; }

        /* How strong the flat background light is, 0 = none (only real lights light things), 1 = the
           old fixed brightness. Saved with the world. */
        float GetAmbientIntensity() const { return m_AmbientIntensity; }
        void SetAmbientIntensity(float value) { m_AmbientIntensity = value < 0.0f ? 0.0f : value; }

    private:
        std::string m_Name;
        std::string m_WorldPath;
        float m_AmbientIntensity = 0.1f;

        entt::registry m_Registry;

        // Moves every playing Animation component forward by dt (looping or stopping at the end).
        void UpdateAnimations(DeltaTime dt);

        bool m_Running = false;

        struct TransformSnapshot { glm::vec3 Position, Rotation, Scale; };
        std::unordered_map<uint64_t, TransformSnapshot> m_RuntimeSnapshot;   // keyed by IDComponent::ID
        std::unordered_map<uint64_t, float> m_AnimationTimeSnapshot;         // same key: Animation time before Play
    };
}
