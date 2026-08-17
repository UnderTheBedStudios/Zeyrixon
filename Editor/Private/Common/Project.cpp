#include <Editor/Public/Common/Project.h>
#include <pugixml.hpp>
#include <stdio.h>

static glm::vec3 ParseVec3(const pugi::xml_node& node)
{
    glm::vec3 v(0.0f);
    if (!node) return v;
    v.x = node.child("d2p1:X").text().as_float();
    v.y = node.child("d2p1:Y").text().as_float();
    v.z = node.child("d2p1:Z").text().as_float();
    return v;
}

bool Project::LoadFromFile(const std::filesystem::path& lumenxPath)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(lumenxPath.c_str());
    if (!result)
    {
        fprintf(stderr, "[Project] Failed to parse %s: %s\n", lumenxPath.string().c_str(), result.description());
        return false;
    }

    pugi::xml_node gameNode = doc.child("Game");
    if (!gameNode)
    {
        fprintf(stderr, "[Project] %s has no <Game> root element\n", lumenxPath.string().c_str());
        return false;
    }

    // The stored <ProjectPath> can go stale if the folder was moved — the actual folder the
    // .lumenx was opened from always wins. We just log a heads-up when they disagree.
    std::string storedProjectPath = gameNode.child("ProjectPath").text().as_string();
    std::filesystem::path actualDirectory = lumenxPath.parent_path();

    if (!storedProjectPath.empty())
    {
        std::error_code ec;
        auto storedCanon = std::filesystem::weakly_canonical(std::filesystem::path(storedProjectPath), ec);
        auto actualCanon = std::filesystem::weakly_canonical(actualDirectory, ec);
        if (storedCanon != actualCanon)
        {
            fprintf(stdout,
                "[Project] Stored ProjectPath (%s) doesn't match where this .lumenx was actually "
                "opened from (%s). Using the actual folder.\n",
                storedProjectPath.c_str(), actualDirectory.string().c_str());
        }
    }

    std::vector<ProjectWorld> worlds;
    pugi::xml_node worldsNode = gameNode.child("Worlds");
    for (pugi::xml_node worldNode : worldsNode.children("World"))
    {
        ProjectWorld w;
        w.name = worldNode.child("WorldName").text().as_string();
        w.active = worldNode.child("Active").text().as_bool();
        w.lightDir = ParseVec3(worldNode.child("LightDir"));
        w.lightColor = ParseVec3(worldNode.child("LightColor"));
        worlds.push_back(std::move(w));
    }

    m_name = gameNode.child("ProjectName").text().as_string();
    m_directory = actualDirectory;
    m_lumenxPath = lumenxPath;
    m_worlds = std::move(worlds);

    return true;
}

const ProjectWorld* Project::ActiveWorld() const
{
    for (auto& w : m_worlds)
        if (w.active)
            return &w;
    return m_worlds.empty() ? nullptr : &m_worlds.front();
}