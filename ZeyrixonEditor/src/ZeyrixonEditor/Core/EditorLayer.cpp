#include <ZeyrixonEditor/Core/EditorLayer.h>
#include <imgui_internal.h>

#include <glad/glad.h>

namespace Editor
{
    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
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

    void EditorLayer::OnAttach() {}
    void EditorLayer::OnDetach() {}

    void EditorLayer::OnUpdate()
    {
        m_Framebuffer->Bind();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
            ImGui::MenuItem("New Project");
            ImGui::MenuItem("Open Project");
            ImGui::MenuItem("Save");
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