#pragma once

#include <Zeyrixon/Core/Layer.h>
#include <Zeyrixon/Events/Event.h>
#include <imgui.h>

#include <Platform/OpenGL/OpenGLFramebuffer.h>
#include <memory>

#include <Zeyrixon/Core/Project.h>

namespace Editor
{
    class EditorLayer : public Zeyrixon::Layer
    {
    public:
        EditorLayer(const std::string& startupProjectPath = std::string());
        ~EditorLayer();

        /* Things that will be used */

        void OnImGuiRender() override;
        void OnEvent(Zeyrixon::Event& event) override;
        void OnUpdate() override;

        /* Things that won't be used but the compiler will yell at me if I don't include them */
        void OnAttach() override;
        void OnDetach() override;

    private:
        std::shared_ptr<Zeyrixon::OpenGLFramebuffer> m_Framebuffer;
        ImVec2 m_ViewportSize = { 0.0f, 0.0f };

        void BuildDefaultLayout(ImGuiID dockspaceId);
        void DrawMenuBar();
        void DrawViewportPanel();
        void DrawWorldOutlinerPanel();
        void DrawPropertiesPanel();
        void DrawContentBrowserPanel();
        void DrawAssetsPanel();

        bool m_IsPlaying = false;
        bool m_ShowGameView = false;

        void DrawProjectLauncher();
        void NewProject();
        void OpenProject();
        void LoadProjectFromPath(const std::string& manifestPath);

        std::shared_ptr<Zeyrixon::Project> m_ActiveProject;
        std::string m_StartupProjectPath;
    };
}