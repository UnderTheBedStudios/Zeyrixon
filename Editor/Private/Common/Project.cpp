#include <Editor/Public/Common/Project.h>
#include <pugixml.hpp>
#include <cstdio>

bool Project::LoadFromFile(const std::filesystem::path& zeyrixonPath)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(zeyrixonPath.c_str());
    if (!result)
    {
        fprintf(stderr, "[Project] Failed to parse %s: %s\n", zeyrixonPath.string().c_str(), result.description());
        return false;
    }

    pugi::xml_node gameNode = doc.child("Game");
    if (!gameNode)
    {
        fprintf(stderr, "[Project] %s has no <Game> root element\n", zeyrixonPath.string().c_str());
        return false;
    }

    // The stored <ProjectPath> can go stale if the folder was moved — the actual folder the
    // .zeyrixon was opened from always wins. We just log a heads-up when they disagree.
    std::string storedProjectPath = gameNode.child("ProjectPath").text().as_string();
    std::filesystem::path actualDirectory = zeyrixonPath.parent_path();

    if (!storedProjectPath.empty())
    {
        std::error_code ec;
        auto storedCanon = std::filesystem::weakly_canonical(std::filesystem::path(storedProjectPath), ec);
        auto actualCanon = std::filesystem::weakly_canonical(actualDirectory, ec);
        if (storedCanon != actualCanon)
        {
            fprintf(stdout,
                "[Project] Stored ProjectPath (%s) doesn't match where this .zeyrixon was actually "
                "opened from (%s). Using the actual folder.\n",
                storedProjectPath.c_str(), actualDirectory.string().c_str());
        }
    }

    std::vector<World> worlds;
    pugi::xml_node worldsNode = gameNode.child("Worlds");
    for (pugi::xml_node worldNode : worldsNode.children("World"))
    {
        World w;
    	w.LoadFromFile(zeyrixonPath.parent_path() / std::filesystem::path(worldNode.child("WorldPath").text().as_string()));
        worlds.push_back(std::move(w));
    }

    m_name = gameNode.child("ProjectName").text().as_string();
    m_directory = actualDirectory;
    m_lumenxPath = zeyrixonPath;
    m_worlds = std::move(worlds);

    return true;
}

const World* Project::ActiveWorld() const
{
    for (auto& w : m_worlds)
        if (w.IsActive())
            return &w;
    return m_worlds.empty() ? nullptr : &m_worlds.front();
}

World* Project::ActiveWorldMutable()
{
    return const_cast<World*>(static_cast<const Project*>(this)->ActiveWorld());
}
