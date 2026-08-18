#pragma once

#include <filesystem>
#include <string>

namespace PathUtils
{
    std::filesystem::path GetDocumentsDirectory();

    std::filesystem::path GetExecutablePath();

    std::filesystem::path ResolveProjectRoot(const std::string& markerFilename = "Zeyrixon.code-workspace");
}