#include <Editor/Public/Common/Window.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <cstdio>

Window::Window(int width, int height, const char* title, WindowType type, bool closeOnEsc)
{
        m_Glsl_Version = nullptr;
    #if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100 (WebGL 1.0)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(IMGUI_IMPL_OPENGL_ES3)
        // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(__APPLE__)
        // GL 3.2 + generally GLSL 150
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);            // Required on Mac
    #else
        // GL 4.6 core to match the shaders (#version 460 core in basic.vert/frag, depth.vert/frag)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    #endif
}

Window::~Window()
{
    DestroyWindow(this);
}

void Window::InitWindow(Window* window, int width, int height, const char* title, WindowType type, bool closeOnEsc)
{
    window->m_WindowHandler = glfwCreateWindow((int)width, (int)height, title, nullptr, nullptr);
    window->m_WindowType = type;

    glfwMakeContextCurrent(window->m_WindowHandler);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "[Window] Failed to initialize GLAD\n");
    }

    glfwSetWindowUserPointer(window->m_WindowHandler, window);
    glfwSetKeyCallback(window->m_WindowHandler, Window::KeyCallbackRoute);
    window->m_CloseOnEsc = closeOnEsc;
}

void Window::DestroyWindow(Window* window)
{
}

void Window::KeyCallbackRoute(GLFWwindow* glfwWindow, int key, int scancode, int action, int mods) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    
    if (windowInstance) {
        windowInstance->HandleKeyInput(key, scancode, action, mods);
    }
}

void Window::HandleKeyInput(int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && m_CloseOnEsc == true) {
        glfwSetWindowShouldClose(m_WindowHandler, GLFW_TRUE);
    }
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(m_WindowHandler); }

void Window::SwapBuffers()
{
    if (m_WindowHandler) glfwSwapBuffers(m_WindowHandler);
}

GLFWwindow* Window::GetWindowHeader(Window* window) const { return window->m_WindowHandler; }

const char* Window::GetGLSLVersion(Window* window) const { return window->m_Glsl_Version; }