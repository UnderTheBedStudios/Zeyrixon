#pragma once

#include <Editor/Public/Windows/Gui/BaseGui.h>
#include <filesystem>
#include <glad/glad.h>

class ContentBrowserRender : public BaseGui
{
public:
    explicit ContentBrowserRender(std::filesystem::path pathToRender);
    ~ContentBrowserRender() override;

    void DrawGui() override;
    static void LoadIcons(const std::filesystem::path& iconDir);

private:
    std::filesystem::path m_pathToRender;

    static GLuint s_openFolderIcon;
    static GLuint s_fileIcon;
    static bool   s_iconsLoaded;
};