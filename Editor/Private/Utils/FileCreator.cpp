#include <Editor/Public/Utils/FileCreator.h>
#include <fstream>
#include <sstream>

namespace FileCreator
{

static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos)
    {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

bool InstantiateProjectFromTemplate(const std::filesystem::path& templateDir,
                                     const std::filesystem::path& targetDir,
                                     const std::string& projectName,
                                     const std::vector<std::string>& folders)
{
    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    if (ec)
        return false;

    for (auto& folder : folders)
    {
        std::filesystem::create_directories(targetDir / folder, ec);
        if (ec)
            return false;
    }

    std::filesystem::path templateZeyrixon = templateDir / "project.zeyrixon";
    if (!std::filesystem::exists(templateZeyrixon))
        return false;

    std::ifstream in(templateZeyrixon);
    if (!in)
        return false;
    std::stringstream buf;
    buf << in.rdbuf();
    std::string content = buf.str();

    std::string projectDirWithSlash = targetDir.string();
    if (projectDirWithSlash.empty() || projectDirWithSlash.back() != '/')
        projectDirWithSlash += '/';

    content = ReplaceAll(content, "{0}", projectName);
    content = ReplaceAll(content, "{1}", projectDirWithSlash);

    std::filesystem::path outFile = targetDir / (projectName + ".zeyrixon");
    std::ofstream out(outFile);
    if (!out)
        return false;
    out << content;

    // Only the screenshot is carried over — icon and the raw template files stay behind.
    std::filesystem::path templateScreenshot = templateDir / "Screenshot.png";
    if (std::filesystem::exists(templateScreenshot))
    {
        std::filesystem::copy_file(templateScreenshot, targetDir / ".Zeyrixon" / "Screenshot.png",
            std::filesystem::copy_options::overwrite_existing, ec);
    }

    // Copy every .zworld file sitting next to project.zeyrixon in the template — these are
    // referenced by <WorldFile> in the manifest we just wrote out above, so without this the
    // new project's manifest points at a file that was never actually created.
    for (const auto& entry : std::filesystem::directory_iterator(templateDir, ec))
    {
        if (entry.path().extension() == ".zworld")
        {
            std::filesystem::copy_file(entry.path(), targetDir / "Content" / "Worlds" /  entry.path().filename(),
                std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    return true;
}

} // namespace FileCreator
