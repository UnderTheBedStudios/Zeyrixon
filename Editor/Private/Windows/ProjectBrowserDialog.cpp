#include <glad/glad.h>
#include <Editor/Public/Windows/ProjectBrowserDialog.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui_internal.h>
#include <filesystem>
#include <Editor/Public/Utils/PathUtils.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <Editor/Public/Utils/FileCreator.h>

#include <stb_image.h>

static GLuint LoadThumbnailTexture(const std::filesystem::path& path, int& outW, int& outH)
{
    if (!std::filesystem::exists(path))
        return 0;

    stbi_set_flip_vertically_on_load(false);

    int channels;
    unsigned char* data = stbi_load(path.string().c_str(), &outW, &outH, &channels, 4);
    if (!data)
        return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

ProjectBrowserDialog::ProjectBrowserDialog(const WindowDesc& desc, Window* parentWindow)
    : Window(desc)
    , m_parentWindow(parentWindow)
{
    // Base Window ctor already handled: glfwCreateWindow (shared GL context via desc.parent),
    // ImGui::CreateContext + SetCurrentContext, ImGui_ImplGlfw_InitForOpenGL, ImGui_ImplOpenGL3_Init.

    MakeCurrent();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    clear_color = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);
}

ProjectBrowserDialog::~ProjectBrowserDialog()
{
    MakeCurrent(); // ensure the right GL context is bound before deleting textures
    for (auto& proj : m_recentProjects)
    {
        if (proj.thumbnailTex != 0)
            glDeleteTextures(1, &proj.thumbnailTex);
    }
}
void ProjectBrowserDialog::DrawFrame()
{
    // If the parent window closed out from under this dialog, mark finished and bail
    // without touching any GL/ImGui state. Caller is responsible for deleting this
    // object once IsFinished() is true.
    if (m_parentWindow && m_parentWindow->ShouldClose())
    {
        m_finished = true;
        return;
    }

    BeginFrame(); // MakeCurrent + ImGui_ImplOpenGL3_NewFrame + ImGui_ImplGlfw_NewFrame + ImGui::NewFrame

    ImGuiWindowFlags windowFlags = 0
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

    {
        

        // TODO: actual project list / "New Project" / "Open" buttons go here.
        // On selection: m_selectedProjectPath = path; m_hasSelectedProject = true; m_finished = true;
        // On cancel: m_finished = true;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags g_windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                    | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoTitleBar
                    | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

        ImGui::Begin("Project Browser", nullptr, g_windowFlags);
        ImGui::PopStyleVar(3);

        const char* tab_labels[] = { "Open Project", "Create Project" };
        const int tab_count = sizeof(tab_labels) / sizeof(tab_labels[0]);

        ImGuiStyle& style = ImGui::GetStyle();
        float total_tabs_width = 0.0f;

        for (int i = 0; i < tab_count; i++)
            total_tabs_width += ImGui::CalcTextSize(tab_labels[i]).x + (style.FramePadding.x * 2.0f);
        total_tabs_width += style.ItemInnerSpacing.x * (tab_count - 1);

        float available_width = ImGui::GetContentRegionAvail().x;
        float center_offset = (available_width - total_tabs_width) * 0.5f;

        if (center_offset > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center_offset);

        if (ImGui::BeginTabBar("Tabs")) 
        {
            static std::vector<std::filesystem::path> recentProjects; // TODO: persist/load from a recents file
            static bool scanned = false;

            if (ImGui::BeginTabItem(tab_labels[0]))
            {
                if (!m_scanned)
                    ScanProjects();

                const float listWidth = 220.0f;
                const float bottomBarHeight = 40.0f;
                ImVec2 avail = ImGui::GetContentRegionAvail();

                // Left: scrollable list
                ImGui::BeginChild("ProjectList", ImVec2(listWidth, avail.y - bottomBarHeight), true);
                for (int i = 0; i < (int)m_recentProjects.size(); i++)
                {
                    auto& proj = m_recentProjects[i];
                    ImGui::PushID(i);

                    bool isSelected = (m_selectedIndex == i);
                    ImVec2 rowStart = ImGui::GetCursorScreenPos();

                    if (ImGui::Selectable("##row", isSelected, 0, ImVec2(0, 48)))
                        m_selectedIndex = i;

                    // Draw icon + name on top of the selectable row
                    ImGui::SetCursorScreenPos(rowStart);
                    if (proj.thumbnailTex != 0)
                        ImGui::Image((ImTextureID)(intptr_t)proj.thumbnailTex, ImVec2(40, 40));
                    else
                        ImGui::Dummy(ImVec2(40, 40)); // placeholder box; swap for a badge/icon draw if you want the "EP" look
                    ImGui::SameLine();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
                    ImGui::Text("%s", proj.name.c_str());

                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Right: large preview of selected project
                ImGui::BeginChild("ProjectPreview", ImVec2(0, avail.y - bottomBarHeight), true);
                if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_recentProjects.size())
                {
                    auto& proj = m_recentProjects[m_selectedIndex];
                    if (proj.thumbnailTex != 0)
                    {
                        ImVec2 previewAvail = ImGui::GetContentRegionAvail();
                        float aspect = (float)proj.thumbW / (float)proj.thumbH;
                        ImVec2 imgSize = previewAvail;
                        if (imgSize.x / aspect < imgSize.y)
                            imgSize.y = imgSize.x / aspect;
                        else
                            imgSize.x = imgSize.y * aspect;
                        ImGui::SetCursorPos(ImVec2(
                            (previewAvail.x - imgSize.x) * 0.5f,
                            (previewAvail.y - imgSize.y) * 0.5f));
                        ImGui::Image((ImTextureID)(intptr_t)proj.thumbnailTex, imgSize);
                    }
                    else
                    {
                        ImGui::TextDisabled("No preview available");
                    }
                }
                ImGui::EndChild();

                // Bottom action bar
                ImGui::Dummy(ImVec2(0, 4));
                bool hasSelection = (m_selectedIndex >= 0 && m_selectedIndex < (int)m_recentProjects.size());
                ImGui::BeginDisabled(!hasSelection);
                if (ImGui::Button("Open Project", ImVec2(120, 0)))
                {
                    m_selectedProjectPath = m_recentProjects[m_selectedIndex].zeyrixonPath.string();
                    m_hasSelectedProject = true;
                    m_finished = true;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Exit", ImVec2(80, 0)))
                {
                    m_finished = true; // no project selected — main.cpp's existing logic will close MainWindow
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(tab_labels[1]))
            {
                if (!m_templatesLoaded)
                    LoadTemplates();

                static char nameBuf[128] = "New Project";
                static char pathBuf[512] = "";
                static bool pathInitialized = false;
                static std::string validationError;

                if (!pathInitialized)
                {
                    pathInitialized = true;
                    std::string defaultPath = (PathUtils::GetDocumentsDirectory() / "Zeyrixon Projects").string();
                    strncpy(pathBuf, defaultPath.c_str(), sizeof(pathBuf) - 1);
                }

                const float listWidth = 200.0f;
                const float formHeight = 100.0f;   // Name + Path rows
                const float bottomBarHeight = 40.0f;
                const float errorHeight = validationError.empty() ? 0.0f : 22.0f;
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float panesHeight = avail.y - formHeight - bottomBarHeight - errorHeight;

                // Left: template list
                ImGui::BeginChild("TemplateList", ImVec2(listWidth, panesHeight), true);
                for (int i = 0; i < (int)m_templates.size(); i++)
                {
                    auto& tmpl = m_templates[i];
                    ImGui::PushID(i);

                    bool isSelected = (m_selectedTemplateIndex == i);
                    ImVec2 rowStart = ImGui::GetCursorScreenPos();

                    if (ImGui::Selectable("##row", isSelected, 0, ImVec2(0, 48)))
                        m_selectedTemplateIndex = i;

                    ImGui::SetCursorScreenPos(rowStart);
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 badgeMin = rowStart;
                    ImVec2 badgeMax = ImVec2(badgeMin.x + 40, badgeMin.y + 40);
                    dl->AddRectFilled(badgeMin, badgeMax, IM_COL32(40, 40, 40, 255), 3.0f);
                    ImVec2 textSize = ImGui::CalcTextSize(tmpl.badge.c_str());
                    dl->AddText(ImVec2(badgeMin.x + (40 - textSize.x) * 0.5f, badgeMin.y + (40 - textSize.y) * 0.5f),
                                IM_COL32(255, 255, 255, 255), tmpl.badge.c_str());

                    ImGui::Dummy(ImVec2(40, 40));
                    ImGui::SameLine();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
                    ImGui::Text("%s", tmpl.name.c_str());

                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Right: preview of selected template
                ImGui::BeginChild("TemplatePreview", ImVec2(0, panesHeight), true);
                if (m_selectedTemplateIndex >= 0 && m_selectedTemplateIndex < (int)m_templates.size())
                {
                    auto& tmpl = m_templates[m_selectedTemplateIndex];
                    if (tmpl.previewTex != 0)
                    {
                        ImVec2 previewAvail = ImGui::GetContentRegionAvail();
                        float aspect = (float)tmpl.previewW / (float)tmpl.previewH;
                        ImVec2 imgSize = previewAvail;
                        if (imgSize.x / aspect < imgSize.y)
                            imgSize.y = imgSize.x / aspect;
                        else
                            imgSize.x = imgSize.y * aspect;
                        ImGui::SetCursorPos(ImVec2(
                            (previewAvail.x - imgSize.x) * 0.5f,
                            (previewAvail.y - imgSize.y) * 0.5f));
                        ImGui::Image((ImTextureID)(intptr_t)tmpl.previewTex, imgSize);
                    }
                    else
                    {
                        ImGui::TextDisabled("No preview available");
                    }
                }
                ImGui::EndChild();

                // Name / Path form
                ImGui::Dummy(ImVec2(0, 8));
                ImGui::Text("Name");
                ImGui::SameLine(80);
                ImGui::SetNextItemWidth(avail.x - 80);
                if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf)))
                    validationError.clear(); // re-validate on next Create press rather than every keystroke

                ImGui::Text("Path");
                ImGui::SameLine(80);
                ImGui::SetNextItemWidth(avail.x - 80 - 70);
                ImGui::InputText("##Path", pathBuf, sizeof(pathBuf));
                ImGui::SameLine();
                if (ImGui::Button("Browse", ImVec2(60, 0)))
                {
                    // TODO: native folder picker
                }

                // Bottom action bar
                ImGui::Dummy(ImVec2(0, 6));
                std::filesystem::path targetDir = std::filesystem::path(pathBuf) / nameBuf;
                bool alreadyExists = std::filesystem::exists(targetDir) && !std::filesystem::is_empty(targetDir);
                bool nameEmpty = (nameBuf[0] == '\0');

                ImGui::BeginDisabled(alreadyExists || nameEmpty);
                if (ImGui::Button("Create", ImVec2(90, 0)))
                {
                    auto& tmpl = m_templates[m_selectedTemplateIndex];

                    // NOTE: this folder list matches every template's template.xml today (.Zeyrixon, Content,
                    // Source Code). If templates ever need different folders, this should read template.xml's
                    // <Folders> element instead of being hardcoded — would need an XML parser (pugixml/tinyxml2)
                    // vendored, since this project doesn't have one yet.
                    static const std::vector<std::string> kStandardFolders = { ".Zeyrixon", "Content", "Source Code" };

                    if (FileCreator::InstantiateProjectFromTemplate(tmpl.templateDir, targetDir, nameBuf, kStandardFolders))
                    {
                        m_selectedProjectPath = (targetDir / (std::string(nameBuf) + ".zeyrixon")).string();
                        m_hasSelectedProject = true;
                        m_finished = true;
                    }
                    else
                    {
                        validationError = "Failed to create project from template.";
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Exit", ImVec2(80, 0)))
                {
                    m_finished = true;
                }

                if (alreadyExists)
                    validationError = "A non-empty directory with the same name already exists.";
                else if (nameEmpty)
                    validationError = "Project name cannot be empty.";

                if (!validationError.empty())
                {
                    ImGui::Dummy(ImVec2(0, 4));
                    ImGui::TextColored(ImVec4(0.9f, 0.25f, 0.25f, 1.0f), "%s", validationError.c_str());
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    int display_w, display_h;
    glfwGetFramebufferSize(Handle(), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    EndFrame(); // ImGui::Render + ImGui_ImplOpenGL3_RenderDrawData + glfwSwapBuffers

    if (ShouldClose())
        m_finished = true;
}

void ProjectBrowserDialog::ScanProjects()
{
    m_scanned = true;
    std::filesystem::path projectsDir = PathUtils::GetDocumentsDirectory() / "Zeyrixon Projects";
    if (!std::filesystem::exists(projectsDir))
        return;

    for (auto& entry : std::filesystem::directory_iterator(projectsDir))
    {
        if (!entry.is_directory())
            continue;

        std::filesystem::path zeyrixonFile;
        for (auto& sub : std::filesystem::directory_iterator(entry.path()))
        {
            if (sub.path().extension() == ".zeyrixon")
            {
                zeyrixonFile = sub.path();
                break;
            }
        }
        if (zeyrixonFile.empty())
            continue;

        ProjectEntry proj;
        proj.name = zeyrixonFile.stem().string();
        proj.zeyrixonPath = zeyrixonFile;
        proj.thumbnailPath = entry.path() / ".Zeyrixon" / "Screenshot.png";
        proj.thumbnailTex = LoadThumbnailTexture(proj.thumbnailPath, proj.thumbW, proj.thumbH);
        m_recentProjects.push_back(std::move(proj));
    }
}

void ProjectBrowserDialog::LoadTemplates()
{
    m_templatesLoaded = true;
    std::filesystem::path templatesRoot = PathUtils::ResolveProjectRoot() / "Editor" / "ProjectTemplates";

    struct TemplateDef { const char* name; const char* badge; const char* folder; };
    static const TemplateDef defs[] = {
        { "Empty Project",       "EP", "EmptyProject" },
        { "First Person Project","FP", "FirstPersonProject" },
        { "Third Person Project","TP", "ThirdPersonProject" },
        { "Top Down Project",    "TD", "TopDownProject" },
    };

    for (auto& def : defs)
    {
        ProjectTemplate t;
        t.name = def.name;
        t.badge = def.badge;
        t.templateDir = templatesRoot / def.folder;
        t.previewPath = t.templateDir / "Screenshot.png";
        t.previewTex = LoadThumbnailTexture(t.previewPath, t.previewW, t.previewH);
        m_templates.push_back(std::move(t));
    }
}