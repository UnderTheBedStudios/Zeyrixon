#include <Editor/Public/Utils/FileCreator.h>
#include <fstream>
#include <sstream>

namespace FileCreator
{

bool WriteZeyrixonPath(const std::filesystem::path& zeyrixonPath,
                      const std::string& projectName,
                      const std::string& projectDirWithSlash)
{
    // unchanged from before — see prior version
    std::ofstream out(zeyrixonPath);
    if (!out)
        return false;

    out << "<Game z:Id=\"i1\" xmlns=\"http://schemas.datacontract.org/2004/07/Zeyrixon.GameProject\"\n"
        "    xmlns:i=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        "    xmlns:z=\"http://schemas.microsoft.com/2003/10/Serialization/\">\n"
        "    <ProjectName>" << projectName << "</ProjectName>\n"
        "    <ProjectPath>" << projectDirWithSlash << "</ProjectPath>\n"
        "    <Worlds>\n"
        "        <World z:Id=\"i2\">\n"
        "            <Active>true</Active>\n"
        "            <WorldName>Default World</WorldName>\n"
        "            <LightDir xmlns:d2p1=\"http://schemas.datacontract.org/2004/07/System.Numerics\">\n"
        "                <d2p1:X>1.0</d2p1:X>\n"
        "                <d2p1:Y>1.0</d2p1:Y>\n"
        "                <d2p1:Z>1.0</d2p1:Z>\n"
        "            </LightDir>\n"
        "            <LightColor xmlns:d2p1=\"http://schemas.datacontract.org/2004/07/System.Numerics\">\n"
        "                <d2p1:X>1.0</d2p1:X>\n"
        "                <d2p1:Y>1.0</d2p1:Y>\n"
        "                <d2p1:Z>1.0</d2p1:Z>\n"
        "            </LightColor>\n"
        "            <_project z:Ref=\"i1\"/>\n"
        "        </World>\n"
        "    </Worlds>\n"
        "</Game>\n";

    return true;
}

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

    return true;
}

} // namespace FileCreator