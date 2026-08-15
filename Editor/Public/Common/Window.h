#pragma once
#include <cstdint>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

enum class WindowType : uint8_t
{
    Normal = 0,
    Dialog = 1
};

class Window
{
public:
    Window(int width = 800, int height = 600, const char* title = "New Window", WindowType type = WindowType::Normal,
           bool closeOnEsc = false);
    ~Window();

    void InitWindow(Window* window, int width = 800, int height = 600, const char* title = "New Window", WindowType type = WindowType::Normal,
           bool closeOnEsc = false);

    void DestroyWindow(Window* window);

    bool ShouldClose() const;
    void SwapBuffers();
    virtual void PollEvents() {};

    GLFWwindow* GetWindowHeader(Window* window) const;
    const char* GetGLSLVersion(Window* window) const;

private:
    GLFWwindow* m_WindowHandler;
    static void KeyCallbackRoute(GLFWwindow* glfwWindow, int key, int scancode, int action, int mods);
    void HandleKeyInput(int key, int scancode, int action, int mods);
    WindowType  m_WindowType;
    bool        m_CloseOnEsc;
    const char* m_Glsl_Version;
};