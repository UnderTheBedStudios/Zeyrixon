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

#include <ZeyrixonEditor/Commands/CommandHistory.h>
#include <ZeyrixonEditor/Commands/CommandsList.h>
#include <optional>
#include <string>

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
        void LoadWorld(std::string worldPath);

    private:
        std::shared_ptr<Zeyrixon::OpenGLFramebuffer> m_Framebuffer;
        ImVec2 m_ViewportSize = { 0.0f, 0.0f };

        EditorCamera m_EditorCamera;
        bool m_ViewportHovered = false;
        ImVec2 m_ViewportMin = { 0.f, 0.f };
        int m_GizmoOperation = 0; // 0 = select (no gizmo), otherwise, it will be ImGuizmo::OPERATION

        void DrawGizmo();
        void DrawLightGizmos();   // icons at each light, plus range / direction outlines for the selected one
        void DrawColliderGizmo(); // drag handles that resize the selected entity's collider (see "Edit Collider")
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
        std::filesystem::path m_ContextAsset;   // the Assets-panel item the context menu was opened on
        char m_RenameBuffer[256] = {};          // text in the Rename dialog
        bool m_OpenRenamePopup = false;         // set by the context menu, consumed by the Assets panel
        std::string m_RenameError;              // shown in the Rename dialog when a rename is rejected

        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FolderClosedIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FolderOpenIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_FileIcon;

        // Viewport light gizmos, compiled into the binary like the icons above.
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_DirectionalLightIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_PointLightIcon;
        std::shared_ptr<Zeyrixon::OpenGLTexture2D> m_SpotLightIcon;
        Zeyrixon::Entity m_HoveredLightGizmo;   // the light icon under the mouse this frame, if any

        // Physics gizmos. Handle ids are axis * 2 + (0 for the + side, 1 for the - side); -1 = none.
        bool m_ShowColliders = true;            // draw every collider as a wireframe in the viewport
        int m_HoveredColliderHandle = -1;       // the resize handle under the mouse this frame
        int m_ActiveColliderHandle = -1;        // the resize handle being dragged
        float m_ColliderGrabOffset = 0.0f;      // how far from the handle's centre it was grabbed, so it doesn't jump

        bool m_IsPlaying = false;
        bool m_ShowGameView = false;

        void DrawProjectLauncher();
        void NewProject();
        void OpenProject();
        void LoadProjectFromPath(const std::string& manifestPath);

        std::shared_ptr<Zeyrixon::Project> m_ActiveProject;
        std::string m_StartupProjectPath;

        std::shared_ptr<Zeyrixon::World> m_ActiveWorld;
        /* Set while a script has switched worlds during Play: the world that was open in the editor, put
           back on Stop so playing never replaces what you were editing. */
        std::shared_ptr<Zeyrixon::World> m_WorldBeforePlaySwitch;
        Zeyrixon::Entity m_SelectedEntity;

        std::vector<Zeyrixon::LogEntry> m_LogEntries;
        uint64_t m_LogVersion = UINT64_MAX;
        bool m_LogAutoScroll = true;
        int m_LogMinLevelIndex = 0;
        ImGuiTextFilter m_LogFilter;

        CommandHistory m_History;

        void Undo();
        void Redo();
        void HandleUndoShortcuts();
        void RecordCreated(Zeyrixon::Entity entity); // call AFTER the new entity is fully set up
        void FlushPendingEdit();
        void ResetHistory();

        // A Properties-panel edit in progress (one drag, or one typing session = one undo step.)
        struct PendingEdit {uint64_t EntityID = 0; std::string Component, Before, After; };
        std::optional<PendingEdit> m_PendingEdit;

        // A gizmo drag in progress.
        bool m_GizmoWasUsing = false;
        uint64_t m_GizmoEntityID = 0;
        std::string m_GizmoBefore;
    };
}