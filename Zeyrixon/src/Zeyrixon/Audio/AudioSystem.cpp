#include <pch.h>
#include <Zeyrixon/Audio/AudioSystem.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/Core/Log.h>
#include <miniaudio.h>
#include <filesystem>

namespace Zeyrixon
{
    struct AudioSystem::Sound
    {
        ma_sound Value{};
        std::string Path;
        bool Initialized = false;
        ~Sound() { if (Initialized) ma_sound_uninit(&Value); }
    };

    AudioSystem::AudioSystem(World& world) : m_World(world)
    {
        auto* engine = new ma_engine{};
        if (ma_engine_init(nullptr, engine) != MA_SUCCESS)
        {
            delete engine;
            Z_CORE_ERROR("Audio: failed to initialize miniaudio device");
            return;
        }
        m_Engine = engine;
        m_Initialized = true;
    }

    AudioSystem::~AudioSystem()
    {
        StopAll();
        if (m_Initialized)
        {
            auto* engine = static_cast<ma_engine*>(m_Engine);
            ma_engine_uninit(engine);
            delete engine;
        }
    }

    bool AudioSystem::Load(uint64_t entityID, const AudioSourceComponent& source, bool start)
    {
        if (!m_Initialized || source.AudioPath.empty())
            return false;
        auto entity = m_World.FindEntityByID(entityID);
        if (!entity)
            return false;

        std::filesystem::path path(source.AudioPath);
        if (path.is_relative())
            path = std::filesystem::path(ScriptEngine::GetAssetRoot()) / path;
        std::error_code ec;
        path = std::filesystem::weakly_canonical(path, ec);
        const std::string fullPath = ec ? source.AudioPath : path.string();

        auto sound = std::make_unique<Sound>();
        auto* engine = static_cast<ma_engine*>(m_Engine);
        const ma_uint32 flags = source.Spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
        const ma_result result = ma_sound_init_from_file(engine, fullPath.c_str(), flags, nullptr, nullptr, &sound->Value);
        if (result != MA_SUCCESS)
        {
            Z_CORE_ERROR("Audio: could not load '{0}' (miniaudio error {1})", fullPath, static_cast<int>(result));
            m_Attempted.insert(entityID);
            return false;
        }
        sound->Initialized = true;
        sound->Path = source.AudioPath;
        ma_sound_set_volume(&sound->Value, std::max(0.0f, source.Volume));
        ma_sound_set_pitch(&sound->Value, std::max(0.01f, source.Pitch));
        ma_sound_set_looping(&sound->Value, source.Looping ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(&sound->Value, source.Spatial ? MA_TRUE : MA_FALSE);
        ma_sound_set_attenuation_model(&sound->Value, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&sound->Value, std::max(0.0f, source.MinDistance));
        ma_sound_set_max_distance(&sound->Value, std::max(source.MinDistance, source.MaxDistance));
        auto [it, inserted] = m_Sounds.insert_or_assign(entityID, std::move(sound));
        (void)inserted;
        if (start && ma_sound_start(&it->second->Value) != MA_SUCCESS)
        {
            Z_CORE_ERROR("Audio: failed to start '{0}'", fullPath);
            return false;
        }
        m_Attempted.insert(entityID);
        return true;
    }

    bool AudioSystem::Play(uint64_t entityID, bool restart)
    {
        auto entity = m_World.FindEntityByID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>())
            return false;
        auto& source = entity.GetComponent<AudioSourceComponent>();
        auto it = m_Sounds.find(entityID);
        if (it == m_Sounds.end() || it->second->Path != source.AudioPath)
            return Load(entityID, source, true);
        if (restart)
            ma_sound_stop(&it->second->Value);
        return ma_sound_start(&it->second->Value) == MA_SUCCESS;
    }

    bool AudioSystem::PlayOneShot(const std::string& audioPath, float volume, float pitch, const glm::vec3* position)
    {
        if (!m_Initialized || audioPath.empty())
            return false;

        std::filesystem::path path(audioPath);
        if (path.is_relative())
            path = std::filesystem::path(ScriptEngine::GetAssetRoot()) / path;
        std::error_code ec;
        path = std::filesystem::weakly_canonical(path, ec);
        const std::string fullPath = ec ? audioPath : path.string();

        auto sound = std::make_unique<Sound>();
        auto* engine = static_cast<ma_engine*>(m_Engine);
        const bool spatial = position != nullptr;
        const ma_uint32 flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
        const ma_result result = ma_sound_init_from_file(engine, fullPath.c_str(), flags, nullptr, nullptr, &sound->Value);
        if (result != MA_SUCCESS)
        {
            Z_CORE_ERROR("Audio: could not load '{0}' (miniaudio error {1})", fullPath, static_cast<int>(result));
            return false;
        }

        sound->Initialized = true;
        sound->Path = audioPath;
        ma_sound_set_volume(&sound->Value, std::max(0.0f, volume));
        ma_sound_set_pitch(&sound->Value, std::max(0.01f, pitch));
        ma_sound_set_spatialization_enabled(&sound->Value, spatial ? MA_TRUE : MA_FALSE);
        if (spatial)
        {
            ma_sound_set_attenuation_model(&sound->Value, ma_attenuation_model_inverse);
            ma_sound_set_position(&sound->Value, position->x, position->y, position->z);
        }
        if (ma_sound_start(&sound->Value) != MA_SUCCESS)
        {
            Z_CORE_ERROR("Audio: failed to start '{0}'", fullPath);
            return false;
        }
        m_OneShots.push_back(std::move(sound));
        return true;
    }

    void AudioSystem::Stop(uint64_t entityID)
    {
        auto it = m_Sounds.find(entityID);
        if (it != m_Sounds.end())
            ma_sound_stop(&it->second->Value);
    }

    void AudioSystem::Remove(uint64_t entityID)
    {
        m_Sounds.erase(entityID);
        m_Attempted.erase(entityID);
    }

    void AudioSystem::StopAll()
    {
        m_Sounds.clear();
        m_OneShots.clear();
        m_Attempted.clear();
    }

    void AudioSystem::SetMasterVolume(float volume)
    {
        if (m_Initialized)
            ma_engine_set_volume(static_cast<ma_engine*>(m_Engine), std::clamp(volume, 0.0f, 1.0f));
    }

    void AudioSystem::Update()
    {
        if (!m_Initialized)
            return;
        auto& registry = m_World.GetRegistry();

        // The primary camera is the listener; camera local -Z is the forward direction.
        auto camera = m_World.GetPrimaryCamera();
        if (camera && camera.HasComponent<TransformComponent>())
        {
            const glm::mat4 matrix = m_World.GetWorldMatrix(camera.Handle());
            glm::vec3 forward = -glm::normalize(glm::vec3(matrix[2]));
            glm::vec3 up = glm::normalize(glm::vec3(matrix[1]));
            const glm::vec3 position = glm::vec3(matrix[3]);
            auto* engine = static_cast<ma_engine*>(m_Engine);
            ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);
            ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);
            ma_engine_listener_set_world_up(engine, 0, up.x, up.y, up.z);
        }

        for (auto [handle, id, source] : registry.view<IDComponent, AudioSourceComponent>().each())
        {
            const uint64_t entityID = id.ID;
            auto it = m_Sounds.find(entityID);
            if (it == m_Sounds.end())
            {
                if (source.PlayOnStart && !source.AudioPath.empty() && !m_Attempted.count(entityID))
                    Load(entityID, source, true);
                continue;
            }
            if (it->second->Path != source.AudioPath)
            {
                Remove(entityID);
                if (!source.AudioPath.empty())
                    Load(entityID, source, source.PlayOnStart);
                continue;
            }

            ma_sound_set_volume(&it->second->Value, std::max(0.0f, source.Volume));
            ma_sound_set_pitch(&it->second->Value, std::max(0.01f, source.Pitch));
            ma_sound_set_looping(&it->second->Value, source.Looping ? MA_TRUE : MA_FALSE);
            ma_sound_set_spatialization_enabled(&it->second->Value, source.Spatial ? MA_TRUE : MA_FALSE);
            ma_sound_set_min_distance(&it->second->Value, std::max(0.0f, source.MinDistance));
            ma_sound_set_max_distance(&it->second->Value, std::max(source.MinDistance, source.MaxDistance));
            if (source.Spatial && registry.all_of<TransformComponent>(handle))
            {
                const glm::vec3 position = m_World.GetWorldPosition(handle);
                ma_sound_set_position(&it->second->Value, position.x, position.y, position.z);
            }
        }

        std::vector<uint64_t> removed;
        for (const auto& [id, sound] : m_Sounds)
        {
            const Entity entity = m_World.FindEntityByID(id);
            if (!entity || !entity.HasComponent<AudioSourceComponent>())
                removed.push_back(id);
        }
        for (uint64_t id : removed) Remove(id);

        for (auto it = m_OneShots.begin(); it != m_OneShots.end();)
        {
            if (ma_sound_at_end(&(*it)->Value))
                it = m_OneShots.erase(it);
            else
                ++it;
        }
    }
}
