#include <glad/glad.h>
#include <Editor/Public/Common/Window.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <Engine/Public/Engine.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

Window::Window(int width, int height, const char* title, WindowType type, bool closeOnEsc)
{
    m_WindowHandler = glfwCreateWindow((int)width, (int)height, title, nullptr, nullptr);
    m_WindowType = type;

    glfwSetWindowUserPointer(m_WindowHandler, this);
    
    glfwSetKeyCallback(m_WindowHandler, Window::KeyCallbackRoute);

    m_CloseOnEsc = closeOnEsc;
}

Window::~Window()
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

void Window::PollEvents()
{
    glfwPollEvents();
}