#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/Components.h>
#include <entt.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Zeyrixon
{
    class World;

    class Z_API AudioSystem
    {
    public:
        explicit AudioSystem(World& world);
        ~AudioSystem();
        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        bool IsInitialized() const { return m_Initialized; }
        bool Play(uint64_t entityID, bool restart = true);
        bool PlayOneShot(const std::string& path, float volume, float pitch,
                         const glm::vec3* position = nullptr);
        void Stop(uint64_t entityID);
        void StopAll();
        void SetMasterVolume(float volume);
        void Update();

    private:
        struct Sound;
        World& m_World;
        void* m_Engine = nullptr;
        bool m_Initialized = false;
        std::unordered_map<uint64_t, std::unique_ptr<Sound>> m_Sounds;
        std::vector<std::unique_ptr<Sound>> m_OneShots;
        std::unordered_set<uint64_t> m_Attempted;
        bool Load(uint64_t entityID, const AudioSourceComponent& source, bool start);
        void Remove(uint64_t entityID);
    };
}
