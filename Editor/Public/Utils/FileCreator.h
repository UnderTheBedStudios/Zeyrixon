#pragma once

#include <filesystem>
#include <string>

namespace FileCreator
{
    bool WriteLumenxFile(const std::filesystem::path& lumenxPath,
                          const std::string& projectName,
                          const std::string& projectDirWithSlash);
}