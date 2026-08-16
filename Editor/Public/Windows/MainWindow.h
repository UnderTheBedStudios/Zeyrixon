#pragma once

#include <Editor/Public/Common/Window.h>
#include <Editor/Public/Common/EditorCamera.h>
#include <glad/glad.h>
#include <imgui.h>

class MainWindow : public Window
{
public:
    explicit MainWindow(const WindowDesc& desc = WindowDesc{});
    ~MainWindow();

    void DrawFrame(int& screenWidth, int& screenHeight);

private:
    GLuint viewportFBO = 0, viewportColorTex = 0, viewportDepthRBO = 0;
    int viewportW = 0, viewportH = 0;

    ImVec4 clear_color;
    EditorCamera editorCamera;
};