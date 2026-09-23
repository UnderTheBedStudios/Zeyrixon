#include <ZeyrixonEditor/Core/EditorLayer.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>

#include <imgui_internal.h>

#include <glad/glad.h>

#include <tinyfiledialogs.h>

#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace
{
    // Splits `directory`'s immediate children into sorted dirs/files lists.
    void ListDirectoryContents(const fs::path& directory,
        std::vector<fs::directory_entry>& outDirs,
        std::vector<fs::directory_entry>& outFiles)
    {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec))
        {
            if (entry.is_directory())
                outDirs.push_back(entry);
            else
                outFiles.push_back(entry);
        }

        auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b)
        {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(outDirs.begin(), outDirs.end(), byName);
        std::sort(outFiles.begin(), outFiles.end(), byName);
    }
}

namespace Editor
{
    EditorLayer::EditorLayer(const std::string& startupProjectPath)
        : Layer("EditorLayer"), m_StartupProjectPath(startupProjectPath)
    {
        Zeyrixon::FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        m_Framebuffer = std::make_shared<Zeyrixon::OpenGLFramebuffer>(spec);

        // Content Browser / Assets panel icons, shipped at the solution root's Icons/ folder.
        fs::path iconsDir = fs::path(Z_ROOT_PATH) / "Icons";
        m_FolderClosedIcon = std::make_shared<Zeyrixon::OpenGLTexture2D>((iconsDir / "ClosedFolder.png").string());
        m_FolderOpenIcon   = std::make_shared<Zeyrixon::OpenGLTexture2D>((iconsDir / "Folder.png").string());
        m_FileIcon         = std::make_shared<Zeyrixon::OpenGLTexture2D>((iconsDir / "File.png").string());
    }

    EditorLayer::~EditorLayer()
    {
    }

    void EditorLayer::BuildDefaultLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID dockMain = dockspaceId;
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.20f, nullptr, &dockMain);
        ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.35f, nullptr, &dockRight);

        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
        ImGuiID dockBottomLeft = ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Left, 0.25f, nullptr, &dockBottom);

        ImGui::DockBuilderDockWindow("Viewport", dockMain);
        ImGui::DockBuilderDockWindow("World", dockRight);
        ImGui::DockBuilderDockWindow("Properties", dockRightBottom);
        ImGui::DockBuilderDockWindow("Content Browser", dockBottomLeft);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorLayer::OnImGuiRender()
    {
        if (!m_ActiveProject)
        {
            DrawProjectLauncher();
            return;
        }

        static bool dockspaceOpen = true;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorDockspaceHost", &dockspaceOpen, hostFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        if (!ImGui::DockBuilderGetNode(dockspaceId))
            BuildDefaultLayout(dockspaceId);

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f));

        DrawMenuBar();

        ImGui::End();

        DrawViewportPanel();
        DrawWorldOutlinerPanel();
        DrawPropertiesPanel();
        DrawContentBrowserPanel();
        DrawAssetsPanel();
    }

    void EditorLayer::DrawProjectLauncher()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 windowSize(420.0f, 160.0f);
        ImGui::SetNextWindowPos(ImVec2(
            viewport->WorkPos.x + (viewport->WorkSize.x - windowSize.x) * 0.5f,
            viewport->WorkPos.y + (viewport->WorkSize.y - windowSize.y) * 0.5f));
        ImGui::SetNextWindowSize(windowSize);

        ImGui::Begin("Zeyrixon", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::TextDisabled("No project loaded.");
        ImGui::Spacing();

        if (ImGui::Button("New Project", ImVec2(-1, 40)))
            NewProject();

        ImGui::Spacing();

        if (ImGui::Button("Open Project", ImVec2(-1, 40)))
            OpenProject();

        ImGui::End();
    }

    void EditorLayer::NewProject()
    {
        const char* parentDir = tinyfd_selectFolderDialog("Choose a location for the new project", nullptr);
        if (!parentDir)
            return;

        const char* name = tinyfd_inputBox("New Project", "Project name:", "MyGame");
        if (!name || name[0] == '\0')
            return;

        m_ActiveProject = Zeyrixon::Project::New(parentDir, name);
        if (m_ActiveProject)
        {
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
            m_SelectedDirectory.clear();
        }
    }

    void EditorLayer::OpenProject()
    {
        // Probe which native dialog backend tinyfd will use, without showing a dialog.
        // "tinyfd_query" as the title makes tinyfd skip rendering anything and just
        // fill tinyfd_response with the backend name it selected (e.g. "zenity", "kdialog").
        tinyfd_messageBox("tinyfd_query", "", "info", "info", 1);
        Z_EDITOR_INFO("tinyfd selected backend: {0}", tinyfd_response);

        const char* filterPatterns[1] = { "*.zeyrixon" };
        const char* path = tinyfd_openFileDialog("Open Project", nullptr, 1, filterPatterns, "Zeyrixon Project", 0);

        if (!path)
            return;

        LoadProjectFromPath(path);
    }

    void EditorLayer::LoadProjectFromPath(const std::string& manifestPath)
    {
        m_ActiveProject = Zeyrixon::Project::Load(manifestPath);
        if (m_ActiveProject)
        {
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
            m_SelectedDirectory.clear();
        }
        else
            Z_EDITOR_ERROR("Failed to open project from: {0}", manifestPath);
    }

    void EditorLayer::OnAttach()
    {
        // Launched via "Open With" / double-click on a .zeyrixon file - the OS
        // hands us the file path as argv[1], which main.cpp forwards down to here.
        // The GL context and window already exist by this point (Application's
        // constructor runs before PushLayer/OnAttach), so it's safe to load here.
        if (!m_StartupProjectPath.empty())
            LoadProjectFromPath(m_StartupProjectPath);
    }

    void EditorLayer::OnDetach() {}

    void EditorLayer::OnUpdate()
    {
        m_Framebuffer->Bind();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Rendering goes here later

        m_Framebuffer->Unbind();
    }

    void EditorLayer::OnEvent(Zeyrixon::Event& e)
    {

    }

    void EditorLayer::DrawMenuBar()
    {
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project")) NewProject();
            if (ImGui::MenuItem("Open Project")) OpenProject();
            if (ImGui::MenuItem("Save", nullptr, false, (bool)m_ActiveProject))
            {
                if (m_ActiveProject)
                    m_ActiveProject->Save();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo");
            ImGui::MenuItem("Redo");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Window")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Help")) { ImGui::EndMenu(); }

        const float rightBlockWidth = 220.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - rightBlockWidth);

        if (ImGui::Button(m_IsPlaying ? "Stop" : "Play", ImVec2(60, 0)))
            m_IsPlaying = !m_IsPlaying;

        ImGui::SameLine();
        if (ImGui::Button(m_ShowGameView ? "Game View" : "Editor View", ImVec2(140, 0)))
            m_ShowGameView = !m_ShowGameView;

        ImGui::EndMenuBar();
    }

    void EditorLayer::DrawViewportPanel()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");
        
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        if (m_ViewportSize.x != viewportPanelSize.x || m_ViewportSize.y != viewportPanelSize.y)
        {
            m_ViewportSize = viewportPanelSize;
            m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
        }

        uint32_t textureID = m_Framebuffer->GetColorAttachmentRenderID();
        ImGui::Image((void*)(intptr_t)textureID, m_ViewportSize, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::DrawWorldOutlinerPanel()
    {
        ImGui::Begin("World");
        if (ImGui::TreeNodeEx("World", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Stuff in World 1");
            ImGui::TreePop();
        }
        ImGui::End();
    }

    void EditorLayer::DrawPropertiesPanel()
    {
        ImGui::Begin("Properties");
        ImGui::TextDisabled("Selected Obj Properties");
        ImGui::End();
    }

    // Renders `directory`'s children as tree nodes (folders) and leaves (files).
    // Folders are only expanded, and their own children listed, when the user
    // opens them - so this stays cheap even for large trees (e.g. Engine).
    void EditorLayer::DrawDirectoryTree(const fs::path& directory)
    {
        std::error_code ec;
        if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec))
        {
            ImGui::TextDisabled("(missing: %s)", directory.string().c_str());
            return;
        }

        std::vector<fs::directory_entry> dirs;
        std::vector<fs::directory_entry> files;
        ListDirectoryContents(directory, dirs, files);

        for (const auto& entry : dirs)
            DrawDirectoryNode(entry.path(), entry.path().filename().string());

        for (const auto& entry : files)
            DrawFileLeaf(entry.path());
    }

    // One folder row: arrow + icon (open/closed) + label. Clicking the row
    // (not just the arrow, thanks to SpanAvailWidth) selects it as the
    // directory the Assets panel shows. `defaultOpen` is only used for the
    // Content Browser's pseudo-roots (Assets/Your Scripts) so real subfolders
    // stay collapsed until asked for.
    void EditorLayer::DrawDirectoryNode(const fs::path& directory, const std::string& label, bool defaultOpen)
    {
        ImGui::PushID(directory.string().c_str());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (defaultOpen)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (m_SelectedDirectory == directory)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx("##dir", flags);
        if (ImGui::IsItemClicked())
            m_SelectedDirectory = directory;

        ImGui::SameLine();
        const auto& folderIcon = open ? m_FolderOpenIcon : m_FolderClosedIcon;
        if (folderIcon && folderIcon->IsLoaded())
            ImGui::Image((void*)(intptr_t)folderIcon->GetRendererID(), ImVec2(16.0f, 16.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());

        if (open)
        {
            DrawDirectoryTree(directory);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // One file row: icon + name, no expand arrow. Mirrors DrawDirectoryNode's
    // call order (hidden TreeNodeEx first, then icon/text via SameLine) so the
    // icon reserves the same arrow-slot spacing a folder row gets - otherwise
    // file icons sit noticeably further left than folder icons at the same depth.
    void EditorLayer::DrawFileLeaf(const fs::path& file)
    {
        ImGui::PushID(file.string().c_str());

        ImGui::TreeNodeEx("##file", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);

        ImGui::SameLine();
        if (m_FileIcon && m_FileIcon->IsLoaded())
            ImGui::Image((void*)(intptr_t)m_FileIcon->GetRendererID(), ImVec2(16.0f, 16.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(file.filename().string().c_str());

        ImGui::PopID();
    }

    void EditorLayer::DrawContentBrowserPanel()
    {
        ImGui::Begin("Content Browser");

        if (m_ActiveProject)
        {
            if (ImGui::TreeNodeEx("Content", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawDirectoryNode(m_ActiveProject->GetAssetDirectory(), "Assets", true);
                DrawDirectoryNode(m_ActiveProject->GetCodeDirectory(), "Your Scripts", true);
                ImGui::TreePop();
            }
        }

        // Engine's own source tree, rooted at the "Zeyrixon" engine module
        // (not the whole solution, which would also pull in ZeyrixonEditor/vendor/.git).
        fs::path engineRoot = fs::path(Z_ROOT_PATH) / "Zeyrixon";
        if (ImGui::TreeNodeEx("Engine", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawDirectoryTree(engineRoot);
            ImGui::TreePop();
        }

        ImGui::End();
    }

    void EditorLayer::DrawAssetsPanel()
    {
        ImGui::Begin("Assets");

        if (!m_ActiveProject)
        {
            ImGui::TextDisabled("No project loaded.");
            ImGui::End();
            return;
        }

        // Nothing picked in the Content Browser yet (or a project was just
        // loaded) - default to the project's Assets folder.
        if (m_SelectedDirectory.empty())
            m_SelectedDirectory = m_ActiveProject->GetAssetDirectory();

        ImGui::TextDisabled("%s", m_SelectedDirectory.string().c_str());
        ImGui::Separator();

        std::error_code ec;
        if (!fs::exists(m_SelectedDirectory, ec) || !fs::is_directory(m_SelectedDirectory, ec))
        {
            ImGui::TextDisabled("(folder no longer exists)");
            ImGui::End();
            return;
        }

        std::vector<fs::directory_entry> dirs;
        std::vector<fs::directory_entry> files;
        ListDirectoryContents(m_SelectedDirectory, dirs, files);

        const float thumbnailSize = 64.0f;
        const float cellPadding = 16.0f;
        const float cellSize = thumbnailSize + cellPadding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        // One grid cell: icon button on top, name wrapped/centered underneath.
        auto drawCell = [&](const fs::path& path, bool isDir)
        {
            ImGui::PushID(path.string().c_str());

            const auto& icon = isDir ? m_FolderClosedIcon : m_FileIcon;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));

            if (icon && icon->IsLoaded())
                ImGui::ImageButton("##thumb", (void*)(intptr_t)icon->GetRendererID(), ImVec2(thumbnailSize, thumbnailSize), ImVec2(0, 1), ImVec2(1, 0));
            else
                ImGui::Button("##thumb", ImVec2(thumbnailSize, thumbnailSize));

            ImGui::PopStyleColor(3);

            // Double-click a folder to navigate into it.
            if (isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_SelectedDirectory = path;

            std::string name = path.filename().string();
            float startX = ImGui::GetCursorPosX();
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            if (textSize.x < thumbnailSize)
                ImGui::SetCursorPosX(startX + (thumbnailSize - textSize.x) * 0.5f);

            ImGui::PushTextWrapPos(startX + thumbnailSize);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopTextWrapPos();

            ImGui::NextColumn();
            ImGui::PopID();
        };

        for (const auto& entry : dirs)
            drawCell(entry.path(), true);

        for (const auto& entry : files)
            drawCell(entry.path(), false);

        ImGui::Columns(1);

        ImGui::End();
    }
}