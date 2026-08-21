#pragma once

#include <glm.hpp>
#include <string>
#include <vector>
#include <filesystem>

// Plain data snapshot of one entity for save/load. No BaseEntity* here on purpose —
// MainWindow is responsible for translating to/from live Engine_* calls.
struct WorldEntity
{
    std::string type;   // matches Engine_CreateEntity's type string, e.g. "Camera", "Cube"
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler degrees
    glm::vec3 scale = glm::vec3(1.0f);
    bool hasModel = false;
    std::string modelPath;
	int index = 0;
};

// Loads/saves a single .zworld file. Owns no engine state — pure data container,
// same rule as Project. Only one of these should be "live" per active world;
// MainWindow is what pushes this into/pulls this from the running Engine.
class World
{
public:
    // Parses zworldPath and populates this object. Returns false (leaving this
    // object unchanged) if the file can't be opened or doesn't look like a valid
    // .zworld file.
    bool LoadFromFile(const std::filesystem::path& zworldPath);

    // Writes this object out to zworldPath (defaults to m_zworldPath if empty).
    bool SaveToFile(std::filesystem::path zworldPath = {});

    [[nodiscard]] const std::string& Name() const { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] const glm::vec3& LightDir() const { return m_lightDir; }
    [[nodiscard]] const glm::vec3& LightColor() const { return m_lightColor; }
    void SetLightDir(const glm::vec3& v) { m_lightDir = v; }
    void SetLightColor(const glm::vec3& v) { m_lightColor = v; }
	void SetActive(const bool active) { m_active = active; }

    // Name of the entity that should become defaultCamera on load. Empty = none set.
    [[nodiscard]] const std::string& DefaultCameraName() const { return m_defaultCameraName; }
    void SetDefaultCameraName(std::string name) { m_defaultCameraName = std::move(name); }

    std::vector<WorldEntity>& Entities() { return m_entities; }
    [[nodiscard]] std::filesystem::path FilePath() const { return m_zworldPath; }

	[[nodiscard]] bool IsActive() const { return m_active; };

private:
    std::filesystem::path m_zworldPath;
    std::string m_name;
    glm::vec3 m_lightDir = glm::vec3(1.0f);
    glm::vec3 m_lightColor = glm::vec3(1.0f);
    std::string m_defaultCameraName;
    std::vector<WorldEntity> m_entities;
	bool m_active = false;
};
