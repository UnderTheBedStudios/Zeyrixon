#include <glad/glad.h>
#include <Editor/Public/Windows/MainWindow.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include <Engine/Public/Engine.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <imgui_internal.h>
#include <Editor/Public/Utils/PathUtils.h>
#include <filesystem>
#include <string>
#include <cstring>

MainWindow::MainWindow(const WindowDesc& desc)
    : Window(desc)
{
    // Base Window ctor already handled: glfwCreateWindow, GL 4.6 core context,
    // ImGui::CreateContext + SetCurrentContext, ImGui_ImplGlfw_InitForOpenGL,
    // ImGui_ImplOpenGL3_Init.

    MakeCurrent();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.WantCaptureKeyboard = false;
    io.WantCaptureMouse = false;

    ImGui::StyleColorsDark();

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;
#endif

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    std::filesystem::path assetRoot = PathUtils::ResolveProjectRoot();
    Engine_Init((void*)glfwGetProcAddress, assetRoot.string().c_str()); // asset root is a placeholder for now

    clear_color = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);
}

MainWindow::~MainWindow()
{
    Engine_Shutdown(); // now that this is implemented — see prior discussion
    // Base ~Window() handles ImGui_ImplOpenGL3_Shutdown, ImGui_ImplGlfw_Shutdown,
    // ImGui::DestroyContext, glfwDestroyWindow.
}

void MainWindow::DrawFrame(int& screenWidth, int& screenHeight)
{
    auto EnsureViewportTarget = [&](int width, int height)
    {
        if (width == viewportW && height == viewportH && viewportFBO != 0) return;
        if (width <= 0 || height <= 0) return;
        if (viewportFBO != 0) {
            glDeleteFramebuffers(1, &viewportFBO);
            glDeleteTextures(1, &viewportColorTex);
            glDeleteRenderbuffers(1, &viewportDepthRBO);
        }
        viewportW = width; viewportH = height;

        glGenTextures(1, &viewportColorTex);
        glBindTexture(GL_TEXTURE_2D, viewportColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenRenderbuffers(1, &viewportDepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, viewportDepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        glGenFramebuffers(1, &viewportFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportColorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, viewportDepthRBO);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fprintf(stderr, "[Editor] Viewport FBO incomplete\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    BeginFrame(); // MakeCurrent + ImGui_ImplOpenGL3_NewFrame + ImGui_ImplGlfw_NewFrame + ImGui::NewFrame

    ImVec2 ViewportSize;
    ImGuiWindowFlags windowFlags = 0
        | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse;

    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags g_windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                    | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoTitleBar
                    | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

        ImGui::Begin("Editor Application", nullptr, g_windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        static bool dockLayoutInitialized = false;
        if (!dockLayoutInitialized)
        {
            dockLayoutInitialized = true;

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

            ImGuiID dockViewport = dockspaceId;
            ImGuiID dockInspector = ImGui::DockBuilderSplitNode(dockViewport, ImGuiDir_Right, 0.15f, nullptr, &dockViewport);
            ImGuiID dockContentBrowser = ImGui::DockBuilderSplitNode(dockViewport, ImGuiDir_Down, 0.25f, nullptr, &dockViewport);
            ImGuiID dockFileExplorer = ImGui::DockBuilderSplitNode(dockContentBrowser, ImGuiDir_Left, 0.20f, nullptr, &dockContentBrowser);
            ImGuiID dockProperties = ImGui::DockBuilderSplitNode(dockInspector, ImGuiDir_Down, 0.5f, nullptr, &dockInspector);
            ImGuiID dockRecentEntities = ImGui::DockBuilderSplitNode(dockViewport, ImGuiDir_Left, 0.15f, nullptr, &dockViewport);

            if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockViewport))
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

            ImGui::DockBuilderDockWindow("Viewport", dockViewport);
            ImGui::DockBuilderDockWindow("Content Browser", dockContentBrowser);
            ImGui::DockBuilderDockWindow("Inspector", dockInspector);
            ImGui::DockBuilderDockWindow("File Explorer", dockFileExplorer);
            ImGui::DockBuilderDockWindow("Properties", dockProperties);
            ImGui::DockBuilderDockWindow("World Properties", dockProperties);
            ImGui::DockBuilderDockWindow("Recent Entities", dockRecentEntities);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        if (ImGui::BeginMenuBar())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            if (ImGui::Button("File")) { ImGui::OpenPopup("File Dropdown"); }
            ImGui::SameLine();
            if (ImGui::Button("Edit")) { ImGui::OpenPopup("Edit Dropdown"); }
            ImGui::PopStyleVar();
            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    {
        if (ImGui::BeginPopup("File Dropdown"))
        {
            if (ImGui::Button("This ")) {  }
            if (ImGui::Button("Does ")) {  }
            if (ImGui::Button("Nothing")) {  }
            if (ImGui::Button("HAHAHAHAHA")) {  }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Edit Dropdown"))
        {
            if (ImGui::Button("Damn")) {  }
            ImGui::EndPopup();
        }
    }

    {
        ImGui::Begin("Viewport", nullptr, windowFlags);

        ViewportSize = ImVec2((int)(screenWidth * .625), (int)(screenHeight * (5.0/6.0)));

        ImGui::SetWindowSize(ViewportSize, ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2((int)(screenWidth * 0.5) - (int)(ImGui::GetWindowSize().x * 0.5), 0), ImGuiCond_FirstUseEver);
        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        ImGuiIO& io = ImGui::GetIO();
        bool viewportHovered = ImGui::IsWindowHovered();

        // Entering a mode requires the click to start over the viewport.
        if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            editorCamera.ProcessMouseButtonPressed(MouseButton::Right);
            glfwSetInputMode(Handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            editorCamera.ProcessMouseButtonPressed(MouseButton::Middle);
            glfwSetInputMode(Handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        // Releasing works regardless of hover, so drags that leave the viewport
        // rect (cursor is hidden/warped anyway) still end cleanly.
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && editorCamera.IsFlying())
        {
            editorCamera.ProcessMouseButtonReleased(MouseButton::Right);
            glfwSetInputMode(Handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) && editorCamera.IsPivoting())
        {
            editorCamera.ProcessMouseButtonReleased(MouseButton::Middle);
            glfwSetInputMode(Handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        if (editorCamera.IsFlying() || editorCamera.IsPivoting())
        {
            editorCamera.ProcessMouseMovement(io.MouseDelta.x, -io.MouseDelta.y, io.DeltaTime);
        }

        if (editorCamera.IsFlying())
        {
            if (ImGui::IsKeyDown(ImGuiKey_W)) editorCamera.ProcessKeyboard(CameraMovement::FORWARD,  io.DeltaTime);
            if (ImGui::IsKeyDown(ImGuiKey_S)) editorCamera.ProcessKeyboard(CameraMovement::BACKWARD, io.DeltaTime);
            if (ImGui::IsKeyDown(ImGuiKey_A)) editorCamera.ProcessKeyboard(CameraMovement::LEFT,     io.DeltaTime);
            if (ImGui::IsKeyDown(ImGuiKey_D)) editorCamera.ProcessKeyboard(CameraMovement::RIGHT,    io.DeltaTime);
            if (ImGui::IsKeyDown(ImGuiKey_E)) editorCamera.ProcessKeyboard(CameraMovement::UP,       io.DeltaTime);
            if (ImGui::IsKeyDown(ImGuiKey_Q)) editorCamera.ProcessKeyboard(CameraMovement::DOWN,     io.DeltaTime);
        }

        EnsureViewportTarget((int)contentSize.x, (int)contentSize.y);

        if (viewportFBO != 0)
        {
            glm::mat4 proj = glm::perspective(glm::radians(editorCamera.fov), contentSize.x / contentSize.y, 0.1f, 100.0f);
            glm::mat4 view = editorCamera.GetViewMatrix();
            glm::mat4 viewProj = proj * view;

            Engine_RenderFrame(viewportFBO, viewportW, viewportH, glm::value_ptr(viewProj));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            ImGui::Image((ImTextureID)(intptr_t)viewportColorTex, contentSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Inspector", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f), (int)((2.0f/3.0f) * ViewportSize.y)), ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2(screenWidth - ImGui::GetWindowSize().x, 0), ImGuiCond_FirstUseEver);

        if (ImGui::Button("Make New Entity"))
            ImGui::OpenPopup("CreateEntityPopup");

        if (ImGui::BeginPopup("CreateEntityPopup"))
        {
            ImGui::TextDisabled("Create Entity");
            ImGui::Separator();

            static const char* entityTypes[] = { "Empty Entity", "Camera" };
            for (const char* type : entityTypes)
            {
                if (ImGui::Selectable(type))
                {
                    CreateEntity(type);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }

        ImGui::Separator();

        int entityCount = Engine_GetEntityCount();
        for (int i = 0; i < entityCount; i++)
        {
            std::string name = Engine_GetEntityName(i);
            std::string type = Engine_GetEntityType(i);
            std::string label = name + " (" + type + ")";

            bool isSelected = (m_selectedEntity == i);
            if (ImGui::Selectable(label.c_str(), isSelected))
                m_selectedEntity = i;
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Content Browser", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(ViewportSize.x), (int)(std::abs(ViewportSize.y - screenHeight))), ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2((int)(screenWidth * 0.5), (int)(ViewportSize.y)), ImGuiCond_FirstUseEver);
        ImGui::End();
    }

    {
        ImGui::Begin("File Explorer", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f),
                                    (int)(std::abs(ViewportSize.y - screenHeight))),
                                    ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2(0, (int)(ViewportSize.y)), ImGuiCond_FirstUseEver);
        ImGui::End();
    }

    {
        ImGui::Begin("Properties", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f),
                                    (int)(std::abs(ViewportSize.y - screenHeight))),
                                    ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2((int)(screenWidth + ImGui::GetWindowSize().x), (int)(ViewportSize.y)), ImGuiCond_FirstUseEver);

        int entityCount = Engine_GetEntityCount();
        if (m_selectedEntity < 0 || m_selectedEntity >= entityCount)
        {
            ImGui::TextDisabled("No entity selected.");
            m_selectedEntity = -1; // clamp — selection can go stale if something else deleted it
        }
        else
        {
            if (m_renameBufferForEntity != m_selectedEntity)
            {
                std::string currentName = Engine_GetEntityName(m_selectedEntity);
                strncpy(m_renameBuffer, currentName.c_str(), sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                m_renameBufferForEntity = m_selectedEntity;
            }

            ImGui::Text("Type: %s", Engine_GetEntityType(m_selectedEntity));

            ImGui::Text("Name");
            if (ImGui::InputText("##EntityName", m_renameBuffer, sizeof(m_renameBuffer)))
                Engine_SetEntityName(m_selectedEntity, m_renameBuffer);

            ImGui::Spacing();
            if (ImGui::Button("Delete Entity"))
            {
                Engine_DeleteEntity(m_selectedEntity);
                m_selectedEntity = -1;
                m_renameBufferForEntity = -1;
            }
        }

        ImGui::End();
    }

    {
        ImGui::Begin("World Properties", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f),
                                    (int)(std::abs(ViewportSize.y - screenHeight))),
                                    ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2((int)(screenWidth + ImGui::GetWindowSize().x), (int)(ViewportSize.y)), ImGuiCond_FirstUseEver);

        if (m_hasProject)
        {
            if (m_project.ActiveWorldMutable() != nullptr)
            {
                std::string currentName = m_project.ActiveWorldMutable()->name;
                strncpy(m_renameWorldBuffer, currentName.c_str(), sizeof(m_renameWorldBuffer) - 1);
                m_renameWorldBuffer[sizeof(m_renameWorldBuffer) - 1] = '\0';

                ImGui::Text("Name");
                if (ImGui::InputText("##WorldName", m_renameWorldBuffer, sizeof(m_renameWorldBuffer)))
                    m_project.ActiveWorldMutable()->name = m_renameWorldBuffer;

                ImGui::Spacing();

                ImGui::Text("Light Direction");
                if (ImGui::DragFloat3("##LightDir", glm::value_ptr(m_project.ActiveWorldMutable()->lightDir), 0.01f))
                {
                    float viewPos[3] = { editorCamera.position.x, editorCamera.position.y, editorCamera.position.z };
                    Engine_SetLight(glm::value_ptr(m_project.ActiveWorldMutable()->lightDir), glm::value_ptr(m_project.ActiveWorldMutable()->lightColor), viewPos);
                }

                ImGui::Text("Light Color");
                if (ImGui::ColorEdit3("##LightColor", glm::value_ptr(m_project.ActiveWorldMutable()->lightColor)))
                {
                    float viewPos[3] = { editorCamera.position.x, editorCamera.position.y, editorCamera.position.z };
                    Engine_SetLight(glm::value_ptr(m_project.ActiveWorldMutable()->lightDir), glm::value_ptr(m_project.ActiveWorldMutable()->lightColor), viewPos);
                }
            }
            else
            {
                ImGui::TextDisabled("There Are No Worlds");
            }
        }
        else
        {
            ImGui::TextDisabled("No Project Selected!");
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Recent Entities", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f),
                                    (int)(ViewportSize.y)),
                                    ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::End();
    }

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    // Clear pass before ImGui draws — base EndFrame() doesn't clear, so do it here.
    int display_w, display_h;
    glfwGetFramebufferSize(Handle(), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    EndFrame(); // ImGui::Render + viewport + ImGui_ImplOpenGL3_RenderDrawData + glfwSwapBuffers
}

void MainWindow::LoadProject(const std::string& zeyrixonPath)
{
    Project proj;
    if (!proj.LoadFromFile(zeyrixonPath))
    {
        fprintf(stderr, "[Editor] Failed to load project: %s\n", zeyrixonPath.c_str());
        return;
    }

    m_project = std::move(proj);
    m_hasProject = true;

    std::string title = "Zeyrixon Editor - " + m_project.Name();
    glfwSetWindowTitle(Handle(), title.c_str());

    if (const ProjectWorld* world = m_project.ActiveWorld())
    {
        float lightDir[3]   = { world->lightDir.x, world->lightDir.y, world->lightDir.z };
        float lightColor[3] = { world->lightColor.x, world->lightColor.y, world->lightColor.z };
        float viewPos[3]    = { editorCamera.position.x, editorCamera.position.y, editorCamera.position.z };
        Engine_SetLight(lightDir, lightColor, viewPos);
    }

    fprintf(stdout, "[Editor] Loaded project \"%s\" from %s\n",
            m_project.Name().c_str(), m_project.Directory().string().c_str());
}

void MainWindow::CreateEntity(const std::string& entityType)
{
    static int s_entityCounter = 0;
    std::string name = entityType + " " + std::to_string(s_entityCounter++);

    int id = Engine_CreateEntity(entityType.c_str(), name.c_str());
    if (id < 0)
        fprintf(stderr, "[Editor] Failed to create entity of type \"%s\"\n", entityType.c_str());
}