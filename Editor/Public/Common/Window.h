#pragma once
#include <string>
#include <functional>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

enum class WindowType
{
    Normal,
    Dialog
};

struct WindowDesc{
    std::string title   = "LumenX";
    int width           = 1280;
    int height          = 720;
    WindowType type     = WindowType::Normal;
    GLFWwindow* parent  = nullptr;
    bool resizable      = true;
    bool decorated      = true;
    bool alwaysOnTop    = false;
    bool centerOnParent = true;
};

class Window
{
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const { return glfwWindowShouldClose(m_handle); }
    void Close()             { glfwSetWindowShouldClose(m_handle, GLFW_TRUE); }

    void MakeCurrent();

    void BeginFrame();
    void EndFrame();

    GLFWwindow*   Handle()   const { return m_handle; }
    ImGuiContext* ImGuiCtx() const { return m_imguiContext; }
    WindowType    Type()     const { return m_type; }

    std::function<void()> OnUI;

private:
    void InitGLFW(const WindowDesc& desc);
    void InitImGui();

    GLFWwindow*   m_handle       = nullptr;
    ImGuiContext* m_imguiContext = nullptr;
    WindowType    m_type;
};