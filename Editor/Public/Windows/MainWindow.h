#pragma once

#include <Editor/Public/Common/Window.h>
#include <Editor/Public/Common/EditorCamera.h>
#include <Editor/Public/Common/Project.h>
#include <glad/glad.h>
#include <imgui.h>

class MainWindow : public Window
{
public:
    explicit MainWindow(const WindowDesc& desc = WindowDesc{});
    ~MainWindow();

    void DrawFrame(int& screenWidth, int& screenHeight);

    void LoadProject(const std::string& lumenxPath);
    bool HasProject() const { return m_hasProject; }
    const Project& CurrentProject() const { return m_project; }

private:
    void CreateEntity(const std::string& entityType);

    GLuint viewportFBO = 0, viewportColorTex = 0, viewportDepthRBO = 0;
    int viewportW = 0, viewportH = 0;

    ImVec4 clear_color;
    EditorCamera editorCamera;

    Project m_project;
    bool m_hasProject = false;
};