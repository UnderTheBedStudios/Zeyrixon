#include <Editor/Public/Windows/Gui/ContentBrowserRender.h>
#include <imgui.h>
#include <stb_image.h>
#include <vector>
#include <algorithm>

#include "Editor/Public/Utils/PathUtils.h"

GLuint ContentBrowserRender::s_openFolderIcon   = 0;
GLuint ContentBrowserRender::s_fileIcon         = 0;
bool   ContentBrowserRender::s_iconsLoaded      = false;

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

ContentBrowserRender::ContentBrowserRender(std::filesystem::path pathToRender)
    :m_pathToRender(pathToRender)
{
}

ContentBrowserRender::~ContentBrowserRender() = default;

void ContentBrowserRender::DrawGui()
{
    ImGui::Text("%s", m_pathToRender.filename().string().c_str());
    ImGui::Separator();

    std::vector<std::filesystem::directory_entry> entries;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(m_pathToRender))
        {
            if (PathUtils::IsHidden(entry.path()))
                continue;
            entries.push_back(entry);
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        ImGui::Text("%s", e.what());
        return;
    }

    std::sort(entries.begin(), entries.end(),
        [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
        {
            bool aDir = a.is_directory();
            bool bDir = b.is_directory();
            if (aDir != bDir)
                return aDir > bDir; // directories first

            std::string an = a.path().filename().string();
            std::string bn = b.path().filename().string();
            std::transform(an.begin(), an.end(), an.begin(), ::tolower);
            std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
            return an < bn;
        });

    const float thumbSize = 64.0f;
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, (int)(panelWidth / (thumbSize + 16.0f)));
    ImGui::Columns(columns, nullptr, false);

    for (const auto& entry : entries)
    {
        GLuint icon = entry.is_directory() ? s_openFolderIcon : s_fileIcon;
        ImGui::PushID(entry.path().string().c_str());
        if (icon)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f)); // subtle hover highlight
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 1, 1, 0.25f)); // subtle click feedback

            ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)icon, ImVec2(thumbSize, thumbSize));

            ImGui::PopStyleColor(3);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && entry.is_directory())
            m_pathToRender = entry.path(); // navigate into it directly via double-click
        ImGui::TextWrapped("%s", entry.path().filename().string().c_str());
        ImGui::PopID();
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

void ContentBrowserRender::LoadIcons(const std::filesystem::path& iconDir)
{
    if (s_iconsLoaded)
        return;

    s_openFolderIcon   = LoadIconTexture(iconDir / "Folder.png");
    s_fileIcon         = LoadIconTexture(iconDir / "File.png");
    s_iconsLoaded = true;
}
