#include <Editor/Public/Utils/FileCreator.h>
#include <fstream>

namespace FileCreator
{

bool WriteLumenxFile(const std::filesystem::path& lumenxPath,
                      const std::string& projectName,
                      const std::string& projectDirWithSlash)
{
    std::ofstream out(lumenxPath);
    if (!out)
        return false;

    out <<
        "<Game z:Id=\"i1\" xmlns=\"http://schemas.datacontract.org/2004/07/LumenX.GameProject\"\n"
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

} // namespace FileCreator