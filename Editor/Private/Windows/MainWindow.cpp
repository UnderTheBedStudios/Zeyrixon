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
            glm::mat4 model = glm::mat4(1.0f);

            Engine_RenderFrame(viewportFBO, viewportW, viewportH, glm::value_ptr(viewProj), glm::value_ptr(model));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            ImGui::Image((ImTextureID)(intptr_t)viewportColorTex, contentSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Inspector", nullptr, windowFlags);
        ImGui::SetWindowSize(ImVec2((int)(std::abs(ViewportSize.x - screenWidth) * 0.5f), (int)((2.0f/3.0f) * ViewportSize.y)), ImGuiCond_FirstUseEver);
        ImGui::SetWindowPos(ImVec2(screenWidth - ImGui::GetWindowSize().x, 0), ImGuiCond_FirstUseEver);
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