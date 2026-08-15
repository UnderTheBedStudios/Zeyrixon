#pragma once

#include <Editor/Public/Common/Window.h>
#include <glad/glad.h>
#include <imgui.h>

class MainWindow : public Window
{
public:
    MainWindow(int width = 800, int height = 600, const char* title = "New Window", WindowType type = WindowType::Normal,
           bool closeOnEsc = false);
    ~MainWindow();

    virtual void PollEvents() override;
    void DrawFrame(int& screenWidth, int& screenHeight);

private:
    GLuint viewportFBO, viewportColorTex, viewportDepthRBO;
    int viewportW, viewportH;

    ImVec4 clear_color;
};