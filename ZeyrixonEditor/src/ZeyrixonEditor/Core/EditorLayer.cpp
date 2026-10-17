#include <ZeyrixonEditor/Core/EditorLayer.h>
#include <Zeyrixon/ImGui/ImGuiLayer.h>
#include <Zeyrixon/Events/MouseEvent.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Picking.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>

#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <tinyfiledialogs.h>

#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
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

    bool DeleteKeyPressed()
    {
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && !ImGui::GetIO().WantTextInput
            && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
    }

    const char* k_LogLevelNames[] = { "Trace", "Info", "Warning", "Error", "Critical" };
    const spdlog::level::level_enum k_LogLevelValues[] = {
        spdlog::level::trace, spdlog::level::info, spdlog::level::warn,
        spdlog::level::err, spdlog::level::critical
    };

    ImVec4 LogLevelColor(spdlog::level::level_enum level)
    {
        switch (level)
        {
            case spdlog::level::trace:
            case spdlog::level::debug:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            case spdlog::level::warn:     return ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
            case spdlog::level::err:      return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            case spdlog::level::critical: return ImVec4(1.0f, 0.15f, 0.55f, 1.0f);
            default:                      return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }
    }

    void SetInspector(const char* name, std::function<void(Zeyrixon::Entity)> draw)
    {
        if (auto* info = Zeyrixon::ComponentRegistry::Get().Find(name))
            info->DrawInspector = std::move(draw);
    }

    // One row of the material inspector: an editable path (applied on Enter), Browse and Clear.
    void DrawTextureSlot(const char* label, std::string& path)
    {
        ImGui::PushID(label);

        char buffer[512]{};
        std::strncpy(buffer, path.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            path = buffer;

        if (ImGui::Button("Browse..."))
        {
            const char* filterPatterns[3] = { "*.png", "*.jpg", "*.jpeg" };
            const char* chosen = tinyfd_openFileDialog("Choose a texture", nullptr, 3, filterPatterns, "Images", 0);
            if (chosen)
                path = chosen;
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            path.clear();

        ImGui::PopID();
    }

    void RegisterComponentInspectors()
    {
        SetInspector("Tag", [](Zeyrixon::Entity entity)
        {
            auto& tag = entity.GetComponent<Zeyrixon::TagComponent>().Tag;

            char buffer[256]{};
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Name", buffer, sizeof(buffer)))
                tag = buffer;
        });

        SetInspector("Transform", [](Zeyrixon::Entity entity)
        {
            auto& transform = entity.GetComponent<Zeyrixon::TransformComponent>();

            ImGui::DragFloat3("Position", &transform.Position.x, 0.1f);

            // Stored in radians, edited in degrees.
            glm::vec3 rotationDegrees = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.5f))
                transform.Rotation = glm::radians(rotationDegrees);

            ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
        });

        SetInspector("Camera", [](Zeyrixon::Entity entity)
        {
            auto& camera = entity.GetComponent<Zeyrixon::CameraComponent>();

            ImGui::Checkbox("Primary", &camera.Primary);
            ImGui::Checkbox("Orthographic", &camera.Orthographic);

            if (camera.Orthographic)
                ImGui::DragFloat("Size", &camera.OrthographicSize, 0.05f, 0.1f, 1000.f);
            else
                ImGui::DragFloat("FOV", &camera.FOV, 0.1f, 1.0f, 179.0f);

            ImGui::DragFloat("Near", &camera.Near, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat("Far", &camera.Far, 1.0f, 1.0f, 100000.0f);
        });

        SetInspector("Mesh", [](Zeyrixon::Entity entity)
        {
            auto& mesh = entity.GetComponent<Zeyrixon::MeshComponent>();

            char buffer[512]{};
            std::strncpy(buffer, mesh.MeshPath.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
                mesh.MeshPath = buffer;

            if (ImGui::Button("Cube"))
                mesh.MeshPath = "builtin:cube";

            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const char* filterPatterns[1] = { "*.obj" };
                const char* chosen = tinyfd_openFileDialog("Choose a mesh", nullptr, 1, filterPatterns, "OBJ model", 0);
                if (chosen)
                    mesh.MeshPath = chosen;
            }
        });

        SetInspector("Sprite", [](Zeyrixon::Entity entity)
        {
            auto& sprite = entity.GetComponent<Zeyrixon::SpriteComponent>();

            DrawTextureSlot("Texture", sprite.TexturePath);

            ImGui::ColorEdit4("Color", &sprite.Color.x);
            ImGui::DragFloat("Pixels Per Unit", &sprite.PixelsPerUnit, 1.0f, 1.0f, 10000.0f);
            ImGui::DragInt("Sorting Order", &sprite.SortingOrder);

            ImGui::Checkbox("Flip X", &sprite.FlipX);
            ImGui::SameLine();
            ImGui::Checkbox("Flip Y", &sprite.FlipY);
            ImGui::SameLine();
            ImGui::Checkbox("Pixel Art", &sprite.PixelArt);
        });

        SetInspector("Script", [](Zeyrixon::Entity entity)
        {
            auto& script = entity.GetComponent<Zeyrixon::ScriptComponent>();

            char pathBuffer[512]{};
            std::strncpy(pathBuffer, script.ScriptPath.c_str(), sizeof(pathBuffer) - 1);
            if (ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer)))
                script.ScriptPath = pathBuffer;

            if (ImGui::Button("Browse..."))
            {
                const char* filterPatterns[1] = { "*.as" };
                const char* chosen = tinyfd_openFileDialog("Choose a script", nullptr, 1, filterPatterns, "AngelScript", 0);
                if (chosen)
                {
                    // Store it relative to the project's Assets folder when it is inside it,
                    // so the project still works if the folder is moved.
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::path relative = fs::relative(chosen, Zeyrixon::ScriptEngine::GetAssetRoot(), ec);
                    const bool inside = !ec && !relative.empty() && *relative.begin() != "..";
                    script.ScriptPath = inside ? relative.generic_string() : std::string(chosen);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear"))
                script.ScriptPath.clear();

            char classBuffer[128]{};
            std::strncpy(classBuffer, script.ClassName.c_str(), sizeof(classBuffer) - 1);
            if (ImGui::InputText("Class", classBuffer, sizeof(classBuffer)))
                script.ClassName = classBuffer;

            ImGui::TextDisabled("Class empty = same name as the file.");
        });

        SetInspector("Material", [](Zeyrixon::Entity entity)
        {
            auto& material = entity.GetComponent<Zeyrixon::MaterialComponent>();

            ImGui::ColorEdit3("Albedo", &material.Albedo.x);
            ImGui::SliderFloat("Metallic", &material.Metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &material.Roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("AO", &material.AO, 0.0f, 1.0f);

            ImGui::SeparatorText("Maps (replace the values above)");
            DrawTextureSlot("Albedo Map", material.AlbedoMap);
            DrawTextureSlot("Normal Map", material.NormalMap);
            DrawTextureSlot("Roughness Map", material.RoughnessMap);
            DrawTextureSlot("Metallic Map", material.MetallicMap);
            DrawTextureSlot("AO Map", material.AOMap);

            ImGui::Separator();
            if (ImGui::Button("Load from .mtl..."))
            {
                const char* filterPatterns[1] = { "*.mtl" };
                const char* chosen = tinyfd_openFileDialog("Choose a material file", nullptr, 1, filterPatterns, "MTL file", 0);
                if (chosen)
                    material.ApplyMTL(chosen);
            }
        });
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

    void EditorLayer::DrawGizmo()
    {
        if (m_ShowGameView)
            return;

        const glm::mat4 view = m_EditorCamera.GetView();
        const glm::mat4 projection = m_EditorCamera.GetProjection();

        ImGuizmo::SetOrthographic(m_EditorCamera.IsOrthographic());
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(m_ViewportMin.x, m_ViewportMin.y, m_ViewportSize.x, m_ViewportSize.y);

        if (!m_SelectedEntity || m_GizmoOperation == 0
            || !m_SelectedEntity.HasComponent<Zeyrixon::TransformComponent>())
            return;

        auto& transform = m_SelectedEntity.GetComponent<Zeyrixon::TransformComponent>();
        glm::vec3 rotationDegrees = glm::degrees(transform.Rotation);

        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(
            glm::value_ptr(transform.Position), glm::value_ptr(rotationDegrees),
            glm::value_ptr(transform.Scale), matrix
        );

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            static_cast<ImGuizmo::OPERATION>(m_GizmoOperation), ImGuizmo::WORLD, matrix
        );

        if (ImGuizmo::IsUsing())
        {
            glm::vec3 position, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(
                matrix, glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale)
            );

            transform.Position = position;
            transform.Rotation = glm::radians(rotation);
            transform.Scale = scale;
        }
    }

    void EditorLayer::DeleteEntity(Zeyrixon::Entity entity)
    {
        if (!m_ActiveWorld || !entity)
            return;

        // Clear the selection first so the properties panel and gizmo never see the deletion
        if (m_SelectedEntity == entity)
            m_SelectedEntity = {};

        m_ActiveWorld->DestroyEntity(entity);
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
        ImGui::DockBuilderDockWindow("Logger", dockBottom);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorLayer::OnImGuiRender()
    {
        ImGuizmo::BeginFrame();

        m_ViewportHovered = false;
        Zeyrixon::Application::Get().GetImGuiLayer()->BlockEvents(true);

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
        DrawLoggerPanel();
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
            LoadStartupWorld();
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
            LoadStartupWorld();
        }
        else
            Z_EDITOR_ERROR("Failed to open project from: {0}", manifestPath);
    }

    void EditorLayer::OnAttach()
    {
        RegisterComponentInspectors();

        if (!m_StartupProjectPath.empty())
            LoadProjectFromPath(m_StartupProjectPath);
    }

    void EditorLayer::OnDetach() {}

    void EditorLayer::OnUpdate(Zeyrixon::DeltaTime dt)
    {
        /* In Game View the editor camera can't start a new drag, but it still finished one
           that's in progress, so the cursor always gets released. */
        m_EditorCamera.OnUpdate(dt, m_ViewportHovered && !m_ShowGameView);

        if (m_IsPlaying && m_ActiveWorld)
            m_ActiveWorld->OnUpdate(dt);

        m_Framebuffer->Bind();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        if (m_ShowGameView)
        {
            // What the primary camera sees. Nothing is drawn if there isn't one.
            Zeyrixon::Entity camera = m_ActiveWorld ? m_ActiveWorld->GetPrimaryCamera() : Zeyrixon::Entity{};
            if (camera)
            {
                const auto& transform = camera.GetComponent<Zeyrixon::TransformComponent>();
                const auto& cameraData = camera.GetComponent<Zeyrixon::CameraComponent>();

                const float aspect = m_ViewportSize.y > 0.f ? m_ViewportSize.x / m_ViewportSize.y : 1.f;

                Zeyrixon::WorldRenderer::RenderWorld(
                    *m_ActiveWorld, transform.GetViewMatrix(), cameraData.GetProjection(aspect), transform.Position
                );

                Zeyrixon::WorldRenderer::RenderSprites(
                    *m_ActiveWorld, transform.GetViewMatrix(), cameraData.GetProjection(aspect)
                );
            }
        }
        else
        {
            const glm::mat4 view = m_EditorCamera.GetView();
            const glm::mat4 projection = m_EditorCamera.GetProjection();

            if (m_ActiveWorld)
            {
                Zeyrixon::WorldRenderer::RenderWorld(
                    *m_ActiveWorld, view, projection, m_EditorCamera.GetPosition()
                );
            }

            glDisable(GL_CULL_FACE);
            // The grid goes last: it's blended over the scene and needs the scene's depth.
            Zeyrixon::WorldRenderer::RenderGrid(view, projection, m_EditorCamera.GetPosition(), m_EditorCamera.Is2D());

            if (m_ActiveWorld)
                Zeyrixon::WorldRenderer::RenderSprites(*m_ActiveWorld, view, projection);
        }

        m_Framebuffer->Unbind();
    }

    void EditorLayer::OnEvent(Zeyrixon::Event& e)
    {
        Zeyrixon::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Zeyrixon::MouseScrolledEvent>([this](Zeyrixon::MouseScrolledEvent& event)
        {
            if (!m_ViewportHovered || m_ShowGameView)
                return false;

            m_EditorCamera.OnMouseScroll(event.GetYOffset());
            return true;
        });
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

                if (m_ActiveWorld && !m_ActiveWorld->Save())
                    Z_EDITOR_ERROR("Failed to save world '{0}'", m_ActiveWorld->GetWorldPath());
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

        if (ImGui::Button(m_IsPlaying ? "Stop" : "Play", ImVec2(60, 0)) && m_ActiveWorld)
        {
            if (m_IsPlaying)
            {
                m_ActiveWorld->OnRuntimeStop();
                m_IsPlaying = false;
            }
            else
            {
                // False means a script failed to compile; the errors are in the Logger panel.
                m_IsPlaying = m_ActiveWorld->OnRuntimeStart();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(m_ShowGameView ? "Game View" : "Editor View", ImVec2(140, 0)))
            m_ShowGameView = !m_ShowGameView;

        ImGui::EndMenuBar();
    }

    void EditorLayer::DrawViewportPanel()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        ImVec2 viewportMin = ImGui::GetCursorScreenPos();
        m_ViewportMin = viewportMin;

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        if (m_ViewportSize.x != viewportPanelSize.x || m_ViewportSize.y != viewportPanelSize.y)
        {
            m_ViewportSize = viewportPanelSize;
            m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
            m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
        }

        uint32_t textureID = m_Framebuffer->GetColorAttachmentRenderID();
        ImGui::Image((void*)(intptr_t)textureID, m_ViewportSize, ImVec2(0, 1), ImVec2(1, 0));

        m_ViewportHovered = ImGui::IsWindowHovered() && !ImGuizmo::IsUsing();

        Zeyrixon::Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered);

        DrawGizmo();

        // No deleting while in Game View (the selection isn't visible there).
        if (!m_ShowGameView && DeleteKeyPressed() && m_SelectedEntity)
            DeleteEntity(m_SelectedEntity);

        // The toolbars only exist in Editor View.
        if (!m_ShowGameView)
        {
            ImVec2 padding = ImVec2(10.0f, 10.0f);
            ImGui::SetCursorScreenPos(ImVec2(viewportMin.x + padding.x, viewportMin.y + padding.y));

            ImGui::BeginGroup();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.8f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                if (ImGui::Button("\uf245"))       { m_GizmoOperation = 0; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F01BE"))   { m_GizmoOperation = ImGuizmo::TRANSLATE; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F0D98"))   { m_GizmoOperation = ImGuizmo::ROTATE; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F0A68"))   { m_GizmoOperation = ImGuizmo::SCALE; }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

            ImGui::EndGroup();

            padding = ImVec2(10.0f, 10.0f);
            ImGui::SetCursorScreenPos(ImVec2(viewportPanelSize.x - padding.x, viewportMin.y + padding.y));

            float currentX = viewportMin.x + viewportPanelSize.x - padding.x;
            float targetY = viewportMin.y + padding.y;

            float itemSpacingX = 10.f;

            ImGui::BeginGroup();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.8f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                float btnWidth1 = 60.0f;
                currentX -= btnWidth1;
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));
                if (ImGui::Button("2D View", ImVec2(btnWidth1, 0)))
                    m_EditorCamera.SetViewType(Editor::EditorCamera::ViewType::View2D);

                float btnWidth2 = 60.0f;
                currentX -= (btnWidth2 + itemSpacingX);
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));
                if (ImGui::Button("3D View", ImVec2(btnWidth2, 0)))
                    m_EditorCamera.SetViewType(Editor::EditorCamera::ViewType::View3D);

                float btnWidth3 = 100.0f;
                currentX -= (btnWidth3 + itemSpacingX);
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));

                // The text after ### is the widget ID, so it stays the same when the label changes.
                const char* viewModeName = m_EditorCamera.IsOrthographic() ? "Orthographic" : "Perspective";
                const std::string viewModeLabel = std::string(viewModeName) + "###ViewMode";

                if (ImGui::Button(viewModeLabel.c_str(), ImVec2(btnWidth3, 0)))
                    ImGui::OpenPopup("ViewModePopup");

                // Open the menu just below the button instead of at the mouse.
                ImGui::SetNextWindowPos(ImVec2(currentX, targetY + ImGui::GetFrameHeight()));
                if (ImGui::BeginPopup("ViewModePopup"))
                {
                    if (ImGui::MenuItem("Perspective", nullptr, !m_EditorCamera.IsOrthographic()))
                        m_EditorCamera.SetProjectionType(Editor::EditorCamera::ProjectionType::Perspective);

                    if (ImGui::MenuItem("Orthographic", nullptr, m_EditorCamera.IsOrthographic()))
                        m_EditorCamera.SetProjectionType(Editor::EditorCamera::ProjectionType::Orthographic);

                    ImGui::EndPopup();
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

            ImGui::EndGroup();
        }
        else
        {
            // Game View overlay: the camera's name, or a hint if there isn't a primary camera.
            Zeyrixon::Entity camera = m_ActiveWorld ? m_ActiveWorld->GetPrimaryCamera() : Zeyrixon::Entity{};
            if (camera)
            {
                const std::string& name = camera.GetComponent<Zeyrixon::TagComponent>().Tag;
                ImGui::SetCursorScreenPos(ImVec2(viewportMin.x + 10.0f, viewportMin.y + 10.0f));
                ImGui::TextDisabled("Game View - %s", name.c_str());
            }
            else
            {
                const char* message = "No primary camera. Add a Camera component to an entity and tick Primary.";
                const ImVec2 textSize = ImGui::CalcTextSize(message);
                ImGui::SetCursorScreenPos(ImVec2(
                    viewportMin.x + (m_ViewportSize.x - textSize.x) * 0.5f,
                    viewportMin.y + m_ViewportSize.y * 0.5f));
                ImGui::TextDisabled("%s", message);
            }
        }

        /* Click to select. Skipped over toolbar buttons and gizmo, and while Alt is held
           (Alt + left drag orbits the camera). */
        if (m_ViewportHovered
            && !m_ShowGameView   // NEW
            && m_ActiveWorld
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered()
            && !ImGuizmo::IsOver()
            && !Zeyrixon::Input::IsKeyPressed(Z_KEY_LEFT_ALT))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const glm::vec2 point(mouse.x - m_ViewportMin.x, mouse.y - m_ViewportMin.y);

            glm::vec3 origin, direction;
            m_EditorCamera.ScreenPointToRay(point, glm::vec2(m_ViewportSize.x, m_ViewportSize.y), origin, direction);

            // An empty click gives an invalid entity, which also deselects.
            m_SelectedEntity = Zeyrixon::Picking::PickEntity(*m_ActiveWorld, origin, direction);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::DrawWorldOutlinerPanel()
    {
        ImGui::Begin("World");

        if (!m_ActiveWorld)
        {
            ImGui::TextDisabled("No world loaded.");
            ImGui::End();
            return;
        }

        // Deleting while the view loop is running can invalidate it, so the
        // actual deletion happens after the loop, using this.
        Zeyrixon::Entity entityToDelete;

        if (ImGui::TreeNodeEx(m_ActiveWorld->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto view = m_ActiveWorld->GetRegistry().view<Zeyrixon::TagComponent>();
            for (auto [handle, tag] : view.each())
            {
                Zeyrixon::Entity entity{ handle, m_ActiveWorld.get() };

                ImGui::PushID(static_cast<int>(static_cast<uint32_t>(handle)));

                if (ImGui::Selectable(tag.Tag.c_str(), m_SelectedEntity == entity))
                    m_SelectedEntity = entity;

                if (ImGui::BeginPopupContextItem())
                {
                    m_SelectedEntity = entity;
                    if (ImGui::MenuItem("Delete Entity"))
                        entityToDelete = entity;
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (DeleteKeyPressed() && m_SelectedEntity)
            entityToDelete = m_SelectedEntity;

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Entity"))
                m_SelectedEntity = m_ActiveWorld->CreateEntity("Entity");

            if (ImGui::MenuItem("Create Sprite"))
            {
                Zeyrixon::Entity sprite = m_ActiveWorld->CreateEntity("Sprite");
                sprite.AddComponent<Zeyrixon::SpriteComponent>();
                m_SelectedEntity = sprite;
            }

            ImGui::EndPopup();
        }

        if (entityToDelete)
            DeleteEntity(entityToDelete);

        ImGui::End();
    }

    void EditorLayer::DrawPropertiesPanel()
    {
        ImGui::Begin("Properties");

        if (!m_SelectedEntity)
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        auto& components = Zeyrixon::ComponentRegistry::Get().GetAll();

        for (auto& info : components)
        {
            if (!info.DrawInspector || !info.Has(m_SelectedEntity))
                continue;

            ImGui::PushID(info.Name.c_str());

            bool open = ImGui::CollapsingHeader(info.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            bool removeRequested = false;
            if (!info.Core && ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Component"))
                    removeRequested = true;
                ImGui::EndPopup();
            }

            if (open)
                info.DrawInspector(m_SelectedEntity);

            if (removeRequested)
                info.Remove(m_SelectedEntity);

            ImGui::PopID();
        }

        ImGui::Separator();

        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponentPopup");

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            for (auto& info : components)
            {
                if (info.Core || info.Has(m_SelectedEntity))
                    continue;

                if (ImGui::MenuItem(info.Name.c_str()))
                    info.Add(m_SelectedEntity);
            }
            ImGui::EndPopup();
        }

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

    void EditorLayer::DrawLoggerPanel()
    {
        ImGui::Begin("Logger");

        auto buffer = Zeyrixon::Log::GetBuffer();
        if (!buffer)
        {
            ImGui::TextDisabled("Logging isn't initialised.");
            ImGui::End();
            return;
        }

        // Only copy the entries again when something new was logged.
        const uint64_t version = buffer->GetVersion();
        if (version != m_LogVersion)
        {
            buffer->CopyEntries(m_LogEntries);
            m_LogVersion = version;
        }

        if (ImGui::Button("Clear"))
            buffer->Clear();

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_LogAutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("##LogLevel", &m_LogMinLevelIndex, k_LogLevelNames, IM_ARRAYSIZE(k_LogLevelNames));

        ImGui::SameLine();
        m_LogFilter.Draw("Filter", 200.0f);

        ImGui::Separator();

        const spdlog::level::level_enum minLevel = k_LogLevelValues[m_LogMinLevelIndex];

        // Which entries pass the level and text filters.
        static std::vector<int> visible;
        visible.clear();
        for (int i = 0; i < static_cast<int>(m_LogEntries.size()); ++i)
        {
            const Zeyrixon::LogEntry& entry = m_LogEntries[i];

            if (entry.Level < minLevel)
                continue;

            if (m_LogFilter.IsActive()
                && !m_LogFilter.PassFilter(entry.Message.c_str())
                && !m_LogFilter.PassFilter(entry.Logger.c_str()))
                continue;

            visible.push_back(i);
        }

        // Scrolls horizontally instead of wrapping, which keeps every row the same height
        // so the clipper below can skip the rows that aren't on screen.
        ImGui::BeginChild("LogScroll", ImVec2(0.0f, 0.0f), 0, ImGuiWindowFlags_HorizontalScrollbar);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const Zeyrixon::LogEntry& entry = m_LogEntries[visible[row]];

                ImGui::PushStyleColor(ImGuiCol_Text, LogLevelColor(entry.Level));
                ImGui::Text("[%s] %s: %s", entry.Time.c_str(), entry.Logger.c_str(), entry.Message.c_str());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        // Stay pinned to the newest message, but only if the view was already at the bottom,
        // so scrolling up to read something isn't yanked away.
        if (m_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();

        ImGui::End();
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

    void EditorLayer::LoadStartupWorld()
    {
        // Replacing the world while it is playing would leave scripts pointing at a dead world.
        if (m_IsPlaying && m_ActiveWorld)
            m_ActiveWorld->OnRuntimeStop();
        m_IsPlaying = false;

        Zeyrixon::ScriptEngine::SetAssetRoot(m_ActiveProject->GetAssetDirectory());

        m_SelectedEntity = {};
        m_ActiveWorld = Zeyrixon::World::Load(m_ActiveProject->GetStartupWorldPath());

        if (!m_ActiveWorld)
            Z_EDITOR_ERROR("Failed to load startup world for project '{0}'", m_ActiveProject->GetName());
    }
}
