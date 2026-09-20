#pragma once

#include <Zeyrixon/Core/Core.h>
#include <memory>
#include <string>

namespace Zeyrixon
{
    class Z_API Project
    {
    public:
        static std::shared_ptr<Project> New(const std::string& parentDirectory, const std::string& name);
        static std::shared_ptr<Project> Load(const std::string& manifestPath);

        bool Save();

        const std::string& GetName() const { return m_Name; }
        const std::string& GetProjectDirectory() const { return m_ProjectDirectory; }
        std::string GetAssetDirectory() const;
        std::string GetCodeDirectory() const;
        std::string GetManifestPath() const;
        
    private:
        Project() = default;

        std::string m_Name;
        std::string m_ProjectDirectory;
        std::string m_AssetRoot = "Assets";
        std::string m_CodeRoot = "src";
        std::string m_StartupWorld = "Worlds/Default.zworld";
    };
}