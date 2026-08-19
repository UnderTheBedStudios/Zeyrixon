#include <Editor/Public/Windows/Gui/FileExplorerRender.h>
#include "imgui.h"
#include "stb_image.h"
#include <vector>
#include <algorithm>

GLuint FileExplorerRender::s_closedFolderIcon = 0;
GLuint FileExplorerRender::s_openFolderIcon   = 0;
GLuint FileExplorerRender::s_fileIcon         = 0;
bool   FileExplorerRender::s_iconsLoaded      = false;

static GLuint LoadIconTexture(const std::filesystem::path& path)
{
    int w, h, channels;
    stbi_set_flip_vertically_on_load(false); // UI icons, not model textures - keep as-authored
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data)
        return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return tex;
}

void FileExplorerRender::LoadIcons(const std::filesystem::path& iconDir)
{
    if (s_iconsLoaded)
        return;

    s_closedFolderIcon = LoadIconTexture(iconDir / "ClosedFolder.png");
    s_openFolderIcon   = LoadIconTexture(iconDir / "Folder.png");
    s_fileIcon         = LoadIconTexture(iconDir / "File.png");
    s_iconsLoaded = true;
}

FileExplorerRender::FileExplorerRender(std::filesystem::path pathToRender)
: m_pathToRender(pathToRender)
{
}

FileExplorerRender::~FileExplorerRender() = default;

void FileExplorerRender::DrawGui()
{
    std::filesystem::path display = m_pathToRender.filename();
    if (display.empty())
        display = m_pathToRender.parent_path().filename();

    DrawEntry(m_pathToRender, true, display.string());
}

void FileExplorerRender::RenderFileExplorerContents(const std::filesystem::path& path)
{
    try
    {
        std::vector<std::filesystem::directory_entry> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path))
            entries.push_back(entry);

        std::sort(entries.begin(), entries.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
            {
                bool aDir = a.is_directory();
                bool bDir = b.is_directory();
                if (aDir != bDir)
                    return aDir > bDir; // directories first

                // case-insensitive alphabetical
                std::string an = a.path().filename().string();
                std::string bn = b.path().filename().string();
                std::transform(an.begin(), an.end(), an.begin(), ::tolower);
                std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
                return an < bn;
            });

        for (const auto& entry : entries)
        {
            DrawEntry(entry.path(), entry.is_directory(), entry.path().filename().string());
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        ImGui::Text("%s", e.what());
    }
}

void FileExplorerRender::DrawEntry(const std::filesystem::path& path, bool isDirectory, const std::string& label)
{
    ImGui::PushID(path.string().c_str());

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID stateId = ImGui::GetID("open");
    bool open = isDirectory && storage->GetBool(stateId, false);

    const float iconSize = ImGui::GetTextLineHeight();
    GLuint icon = isDirectory ? (open ? s_openFolderIcon : s_closedFolderIcon) : s_fileIcon;

    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    // Full-width clickable row (transparent - just for hit testing/highlight)
    bool clicked = ImGui::Selectable("##row", false,
                                      ImGuiSelectableFlags_AllowOverlap,
                                      ImVec2(0, iconSize));

    // Draw icon + label on top of the selectable
    ImGui::SetCursorScreenPos(rowStart);
    if (icon)
    {
        ImGui::Image((ImTextureID)(intptr_t)icon, ImVec2(iconSize, iconSize));
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(label.c_str());

    if (clicked && isDirectory)
        storage->SetBool(stateId, !open);

    ImGui::PopID();

    if (isDirectory && open)
    {
        ImGui::Indent();
        RenderFileExplorerContents(path);
        ImGui::Unindent();
    }
}