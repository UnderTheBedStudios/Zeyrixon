#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <Engine/Public/Common/World.h>

class Project
{
public:
    // Parses zeyrixonPath and populates this object. Returns false (leaving this object
    // unchanged) if the file can't be opened or doesn't look like a valid .lumenx file.
    bool LoadFromFile(const std::filesystem::path& zeyrixonPath);

    [[nodiscard]] const std::string& Name() const { return m_name; }
    [[nodiscard]] const std::filesystem::path& Directory() const { return m_directory; }
    [[nodiscard]] const std::filesystem::path& LumenxPath() const { return m_lumenxPath; }
    [[nodiscard]] const std::vector<World>& Worlds() const { return m_worlds; }

    // First World with Active=true, falling back to the first World if none are marked active,
    // or nullptr if there are no Worlds at all.
    [[nodiscard]] const World* ActiveWorld() const;

    World* ActiveWorldMutable();

private:
    std::string m_name;
    std::filesystem::path m_directory;  // actual folder the .lumenx was opened from — authoritative
                                         // over whatever <ProjectPath> says inside the file, since
                                         // that field goes stale if the project folder is moved.
    std::filesystem::path m_lumenxPath;
    std::vector<World> m_worlds;
};
