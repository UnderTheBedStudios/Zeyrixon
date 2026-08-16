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

#include <stb_image.h>

static GLuint LoadThumbnailTexture(const std::filesystem::path& path, int& outW, int& outH)
{
    if (!std::filesystem::exists(path))
        return 0;

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
                    m_selectedProjectPath = m_recentProjects[m_selectedIndex].lumenxPath.string();
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
                static char nameBuf[128] = "NewProject";
                static char locationBuf[512] = "";
                static bool locationInitialized = false;

                if (!locationInitialized)
                {
                    locationInitialized = true;
                    std::string defaultLoc = (PathUtils::GetDocumentsDirectory() / "LumenX Projects").string();
                    strncpy(locationBuf, defaultLoc.c_str(), sizeof(locationBuf) - 1);
                }

                ImGui::Spacing();
                ImGui::Text("Project Name");
                ImGui::InputText("##ProjectName", nameBuf, sizeof(nameBuf));

                ImGui::Spacing();
                ImGui::Text("Location");
                ImGui::InputText("##Location", locationBuf, sizeof(locationBuf));
                ImGui::SameLine();
                if (ImGui::Button("Browse##Location"))
                {
                    // TODO: native folder picker
                }

                ImGui::Spacing();
                if (ImGui::Button("Create"))
                {
                    std::filesystem::path projectDir = std::filesystem::path(locationBuf) / nameBuf;
                    std::error_code ec;
                    std::filesystem::create_directories(projectDir, ec);

                    if (!ec)
                    {
                        std::filesystem::path lumenxFile = projectDir / (std::string(nameBuf) + ".lumenx");
                        std::ofstream out(lumenxFile);
                        if (out)
                        {
                            out << "<Project Name=\"" << nameBuf << "\" />\n";
                            out.close();

                            m_selectedProjectPath = lumenxFile.string();
                            m_hasSelectedProject = true;
                            m_finished = true;
                        }
                        else
                        {
                            fprintf(stderr, "[ProjectBrowser] Failed to write %s\n", lumenxFile.string().c_str());
                        }
                    }
                    else
                    {
                        fprintf(stderr, "[ProjectBrowser] Failed to create directory %s: %s\n",
                                projectDir.string().c_str(), ec.message().c_str());
                    }
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
    std::filesystem::path projectsDir = PathUtils::GetDocumentsDirectory() / "LumenX Projects";
    if (!std::filesystem::exists(projectsDir))
        return;

    for (auto& entry : std::filesystem::directory_iterator(projectsDir))
    {
        if (!entry.is_directory())
            continue;

        std::filesystem::path lumenxFile;
        for (auto& sub : std::filesystem::directory_iterator(entry.path()))
        {
            if (sub.path().extension() == ".lumenx")
            {
                lumenxFile = sub.path();
                break;
            }
        }
        if (lumenxFile.empty())
            continue;

        ProjectEntry proj;
        proj.name = lumenxFile.stem().string();
        proj.lumenxPath = lumenxFile;
        proj.thumbnailPath = entry.path() / ".LumenX/thumbnail.png";
        proj.thumbnailTex = LoadThumbnailTexture(proj.thumbnailPath, proj.thumbW, proj.thumbH);
        m_recentProjects.push_back(std::move(proj));
    }
}