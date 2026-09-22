#include <pch.h>

#include <Platform/OpenGL/OpenGLContext.h>
#include <Zeyrixon/Core/Log.h>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Zeyrixon
{
    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle)
    {
        if (!m_WindowHandle)
            Z_CORE_CRITICAL("OpenGLContext was created with a null window handle! Fix this immedietly!");
    }

    void OpenGLContext::Init()
    {
        glfwMakeContextCurrent(m_WindowHandle);
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

        if (!status)
            Z_CORE_CRITICAL("Failed to initialize Glad: {0}", status);
        else
            Z_CORE_INFO("Glad loaded successfully: {0}", status);
    }

    void OpenGLContext::SwapBuffers()
    {
        glfwSwapBuffers(m_WindowHandle);
    }
}