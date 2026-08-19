#pragma once

#include <Editor/Public/Windows/Gui/BaseGui.h>
#include <filesystem>
#include <glad/glad.h>

class FileExplorerRender : public BaseGui
{
public:
    explicit FileExplorerRender(std::filesystem::path path);
    ~FileExplorerRender() override;

    void DrawGui() override;
    void RenderFileExplorerContents(const std::filesystem::path& path);

    // Call once, after a GL context exists, before any FileExplorerRender::DrawGui().
    static void LoadIcons(const std::filesystem::path& iconDir);

private:
    void DrawEntry(const std::filesystem::path& path, bool isDirectory, const std::string& label);

    std::filesystem::path m_pathToRender;

    static GLuint s_closedFolderIcon;
    static GLuint s_openFolderIcon;
    static GLuint s_fileIcon;
    static bool   s_iconsLoaded;
};