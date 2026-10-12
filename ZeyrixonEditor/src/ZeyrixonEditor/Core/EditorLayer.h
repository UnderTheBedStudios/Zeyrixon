#pragma once

#include <Zeyrixon/Core/Layer.h>
#include <Zeyrixon/Events/Event.h>
#include <ZeyrixonEditor/Core/EditorCamera.h>
#include <Zeyrixon/Core/Log.h>
#include <imgui.h>

#include <Platform/OpenGL/OpenGLFramebuffer.h>
#include <Platform/OpenGL/OpenGLTexture.h>
#include <memory>
#include <filesystem>

#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>

#include <Zeyrixon/Core/DeltaTime.h>

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
        void OnUpdate(Zeyrixon::DeltaTime dt) override;

        /* Things that won't be used but the compiler will yell at me if I don't include them */
        void OnAttach() override;
        void OnDetach() override;

        void LoadStartupWorld();

    private:
        std::shared_ptr<Zeyrixon::OpenGLFramebuffer> m_Framebuffer;
        ImVec2 m_ViewportSize = { 0.0f, 0.0f };

        EditorCamera m_EditorCamera;
        bool m_ViewportHovered = false;
        ImVec2 m_ViewportMin = { 0.f, 0.f };
        int m_GizmoOperation = 0; // 0 = select (no gizmo), otherwise, it will be ImGuizmo::OPERATION

        void DrawGizmo();
        void DeleteEntity(Zeyrixon::Entity entity);

        void BuildDefaultLayout(ImGuiID dockspaceId);
        void DrawMenuBar();
        void DrawViewportPanel();
        void DrawWorldOutlinerPanel();
        void DrawPropertiesPanel();
        void DrawLoggerPanel();
        void DrawContentBrowserPanel();
        void DrawAssetsPanel();
        void DrawDirectoryTree(const std::filesystem::path& directory);
        void DrawDirectoryNode(const std::filesystem::path& directory, const std::string& label, bool defaultOpen = false);
        void DrawFileLeaf(const std::filesystem::path& file);

        std::filesystem::path m_SelectedDirectory;

        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FolderClosedIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FolderOpenIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FileIcon;

        bool m_IsPlaying = false;
        bool m_ShowGameView = false;

        void DrawProjectLauncher();
        void NewProject();
        void OpenProject();
        void LoadProjectFromPath(const std::string& manifestPath);

        std::shared_ptr<Zeyrixon::Project> m_ActiveProject;
        std::string m_StartupProjectPath;

        std::shared_ptr<Zeyrixon::World> m_ActiveWorld;
        Zeyrixon::Entity m_SelectedEntity;

        std::vector<Zeyrixon::LogEntry> m_LogEntries;
        uint64_t m_LogVersion = UINT64_MAX;
        bool m_LogAutoScroll = true;
        int m_LogMinLevelIndex = 0;
        ImGuiTextFilter m_LogFilter;
    };
}
