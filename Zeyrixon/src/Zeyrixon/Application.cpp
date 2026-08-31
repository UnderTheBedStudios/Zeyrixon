#include <pch.h>
#include <Zeyrixon/Application.h>

#include <GLFW/glfw3.h>

namespace Zeyrixon
{
    Application::Application()
    {
        m_Window = std::unique_ptr<Window>(Window::Create());
    }

    Application::~Application()
    {
    }

    /* This is meant to make the app go vroom vroom :) */
    void Application::Run()
    {
        while (m_Running)
        {
            glClearColor(1, 0, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            m_Window->OnUpdate();
        }
    }
}