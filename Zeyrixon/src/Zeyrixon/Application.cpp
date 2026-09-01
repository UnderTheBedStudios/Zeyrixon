#include <pch.h>
#include <Zeyrixon/Application.h>
#include <Zeyrixon/Log.h>
#include <string>

#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Zeyrixon
{
#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

    Application::Application()
    {
        m_Window = std::shared_ptr<Window>(Window::Create());
        m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));
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

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);

        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;
        return true;
    }

    void Application::ChangeWindowImage(const char* path)
    {
        GLFWimage images[1];

        std::string full_path = Z_PROJECT_ROOT;
        full_path += path;

        images[0].pixels = stbi_load(full_path.c_str(), &images[0].width, &images[0].height, 0, 4);

        if (images[0].pixels)
            glfwSetWindowIcon(GetWindow()->GetWindow(), 1, images);
        else
            Z_CORE_CRITICAL("Failed to load image for window Image at: {0}", full_path.c_str());

        stbi_image_free(images[0].pixels);
    }

    void Application::ChangeWindowTitle(const char* name)
    {
        glfwSetWindowTitle(GetWindow()->GetWindow(), name);
    }
}