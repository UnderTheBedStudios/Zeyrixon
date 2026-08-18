#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace FileCreator
{
    // Old direct writer — kept for now in case anything else calls it, but Create Project
    // should prefer InstantiateProjectFromTemplate below.
    bool WriteZeyrixonFile(const std::filesystem::path& zeyrixonPath,
                          const std::string& projectName,
                          const std::string& projectDirWithSlash);

    // Reads <templateDir>/project.zeyrixon, substitutes {0}=projectName and {1}=projectDirWithSlash,
    // and writes the result to <targetDir>/<projectName>.zeyrixon.
    // folders is the set of subfolders to create under targetDir (currently hardcoded to match
    // every template's template.xml — see note in ProjectBrowserDialog.cpp).
    bool InstantiateProjectFromTemplate(const std::filesystem::path& templateDir,
                                         const std::filesystem::path& targetDir,
                                         const std::string& projectName,
                                         const std::vector<std::string>& folders);
}