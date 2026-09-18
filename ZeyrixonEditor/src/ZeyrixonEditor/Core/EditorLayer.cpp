#include <ZeyrixonEditor/Core/EditorLayer.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>

#include <imgui_internal.h>

#include <glad/glad.h>

#include <tinyfiledialogs.h>

namespace Editor
{
    EditorLayer::EditorLayer(const std::string& startupProjectPath)
        : Layer("EditorLayer"), m_StartupProjectPath(startupProjectPath)
    {
        Zeyrixon::FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        m_Framebuffer = std::make_shared<Zeyrixon::OpenGLFramebuffer>(spec);
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
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
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
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
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

    void EditorLayer::DrawContentBrowserPanel()
    {
        ImGui::Begin("Content Browser");
        if (ImGui::TreeNodeEx("Content", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::TreeNodeEx("Your Stuff", ImGuiTreeNodeFlags_DefaultOpen))
                ImGui::TreePop();

            if (ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Your Code Editor");
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::End();
    }

    void EditorLayer::DrawAssetsPanel()
    {
        ImGui::Begin("Assets");
        ImGui::TextDisabled("Things in currently selected folder");
        ImGui::End();
    }
}