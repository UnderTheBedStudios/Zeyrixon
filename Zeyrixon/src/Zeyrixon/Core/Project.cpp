#include <pch.h>
#include <Zeyrixon/Core/Project.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/Core/Log.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    using Tokens = std::vector<std::pair<std::string, std::string>>;

    std::string ToForwardSlashes(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    // "My Game!" -> "My_Game_": usable as a premake project name and a folder name.
    std::string MakeIdentifier(const std::string& name)
    {
        std::string id = name;
        for (char& c : id)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                c = '_';
        }
        if (id.empty() || std::isdigit(static_cast<unsigned char>(id[0])))
            id.insert(id.begin(), '_');
        return id;
    }

    /* Copies a template file to its place in the new project, replacing every @TOKEN@. It never
       overwrites: if the file is already there it is the user's, and they may have edited it. */
    bool WriteFromTemplate(const fs::path& templatePath, const fs::path& outPath, const Tokens& tokens,
                           bool executable = false)
    {
        if (fs::exists(outPath))
            return true;

        std::ifstream in(templatePath, std::ios::binary);
        if (!in.is_open())
        {
            Z_CORE_WARN("Project template missing: {0}", templatePath.string());
            return false;
        }

        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string text = buffer.str();

        for (const auto& [token, value] : tokens)
        {
            const std::string marker = "@" + token + "@";
            for (size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at + value.size()))
                text.replace(at, marker.size(), value);
        }

        fs::create_directories(outPath.parent_path());
        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open())
            return false;
        out << text;
        out.close();

        if (executable)
        {
            std::error_code ec;   // not fatal: on Windows this does nothing useful
            fs::permissions(outPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                            fs::perm_options::add, ec);
        }
        return true;
    }

    /* Gives a new project everything it needs to be built as a game: a premake5.lua that
       connects it to the engine, a main.cpp, scripts and scripts to generate the build files. */
    void WriteGameProjectFiles(const std::string& name, const fs::path& projectDirectory,
                               const fs::path& assetDirectory, const fs::path& codeDirectory,
                               const fs::path& manifestPath)
    {
        std::string engineDir = ToForwardSlashes(Z_ROOT_PATH);
        while (!engineDir.empty() && engineDir.back() == '/')
            engineDir.pop_back();

        const fs::path templates = fs::path(engineDir) / "templates" / "GameProject";

        const Tokens tokens =
        {
            { "PROJECT_NAME", name },
            { "PROJECT_ID", MakeIdentifier(name) },
            { "ZEYRIXON_DIR", engineDir },
            { "MANIFEST_PATH", ToForwardSlashes(fs::absolute(manifestPath).string()) },
        };

        WriteFromTemplate(templates / "premake5.lua.in", projectDirectory / "premake5.lua", tokens);
        WriteFromTemplate(templates / "GenerateProjects.sh.in", projectDirectory / "GenerateProjects.sh", tokens, true);
        WriteFromTemplate(templates / "GenerateProjects.bat.in", projectDirectory / "GenerateProjects.bat", tokens);
        WriteFromTemplate(templates / "gitignore.in", projectDirectory / ".gitignore", tokens);
        WriteFromTemplate(templates / "main.cpp.in", codeDirectory / "main.cpp", tokens);
        WriteFromTemplate(templates / "Example.as.in", assetDirectory / "Scripts" / "Example.as", tokens);
    }
}

namespace Zeyrixon
{
    std::shared_ptr<Project> Project::New(const std::string& parentDirectory, const std::string& name)
    {
        auto project = std::shared_ptr<Project>(new Project());
        project->m_Name = name;
        project->m_ProjectDirectory = (fs::path(parentDirectory) / name).string();

        fs::create_directories(project->m_ProjectDirectory);
        fs::create_directories(fs::path(project->m_ProjectDirectory) / project->m_AssetRoot);
        fs::create_directories(fs::path(project->m_ProjectDirectory) / project->m_AssetRoot / "Worlds");
        fs::create_directories(fs::path(project->m_ProjectDirectory) / project->m_CodeRoot);
        fs::create_directories(fs::path(project->m_ProjectDirectory) / project->m_AssetRoot / "Scripts");

        auto defaultWorld = World::New("Default");
        std::string worldPath = project->GetStartupWorldPath();
        if (!defaultWorld->SaveAs(worldPath))
        {
            Z_CORE_CRITICAL("Failed to write default world for project '{0}'", name);
            return nullptr;
        }
        if (!project->Save())
        {
            Z_CORE_CRITICAL("Failed to write project manifest for '{0}'", name);
            return nullptr;
        }

        WriteGameProjectFiles(name, project->m_ProjectDirectory, project->GetAssetDirectory(),
                              project->GetCodeDirectory(), project->GetManifestPath());

        Z_CORE_INFO("Created project '{0}' at {1}", name, project->m_ProjectDirectory);
        return project;
    }

    std::shared_ptr<Project> Project::Load(const std::string& manifestPath)
    {
        std::ifstream in(manifestPath);
        if (!in.is_open())
        {
            Z_CORE_ERROR("Could not open project manifest: {0}", manifestPath);
            return nullptr;
        }

        auto project = std::shared_ptr<Project>(new Project());
        project->m_ProjectDirectory = fs::path(manifestPath).parent_path().string();

        std::string line;
        while (std::getline(in, line))
        {
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);

            if (key == "Name")              project->m_Name         = value;
            else if (key == "AssetRoot")    project->m_AssetRoot    = value;
            else if (key == "StartupWorld") project->m_StartupWorld = value;
        }

        if (project->m_Name.empty())
        {
            Z_CORE_ERROR("Project manifest '{0}' is missing a Name entry", manifestPath);
            return nullptr;
        }

        Z_CORE_INFO("Loaded project '{0}' from {1}", project->m_Name, project->m_ProjectDirectory);
        return project;
    }

    bool Project::Save()
    {
        std::ofstream out(GetManifestPath());
        if (!out.is_open())
            return false;

        out << "Name=" << m_Name << "\n";
        out << "AssetRoot=" << m_AssetRoot << "\n";
        out << "CodeRoot=" << m_CodeRoot << "\n";
        out << "StartupWorld=" << m_StartupWorld << "\n";

        return true;
    }

    std::string Project::GetAssetDirectory() const
    {
        return (fs::path(m_ProjectDirectory) / m_AssetRoot).string();
    }

    std::string Project::GetCodeDirectory() const
    {
        return (fs::path(m_ProjectDirectory) / m_CodeRoot).string();
    }

    std::string Project::GetManifestPath() const
    {
        return (fs::path(m_ProjectDirectory) / (m_Name + ".zeyrixon")).string();
    }

    std::string Project::GetStartupWorldPath() const
    {
        return (fs::path(GetAssetDirectory()) / m_StartupWorld).string();
    }
}
