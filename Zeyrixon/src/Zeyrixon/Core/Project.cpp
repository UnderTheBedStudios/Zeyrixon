#include <pch.h>
#include <Zeyrixon/Core/Project.h>
#include <Zeyrixon/Core/Log.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Zeyrixon
{
    std::shared_ptr<Project> Project::New(const std::string& parentDirectory, const std::string& name)
    {
        auto project = std::shared_ptr<Project>(new Project());
        project->m_Name = name;
        project->m_ProjectDirectory = (fs::path(parentDirectory) / name).string();

        fs::create_directories(project->m_ProjectDirectory);
        fs::create_directories(fs::path(project->m_ProjectDirectory) / project->m_AssetRoot);
        fs::create_directories(fs::path(project->m_ProjectDirectory) / "Worlds");

        if (!project->Save())
        {
            Z_CORE_CRITICAL("Failed to write project manifest for '{0}'", name);
            return nullptr;
        }

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
        out << "StartupWorld=" << m_StartupWorld << "\n";

        return true;
    }

    std::string Project::GetAssetDirectory() const
    {
        return (fs::path(m_ProjectDirectory) / m_AssetRoot).string();
    }

    std::string Project::GetManifestPath() const
    {
        return (fs::path(m_ProjectDirectory) / (m_Name + ".zeyrixon")).string();
    }
}