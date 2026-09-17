#include <pch.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/ImGui/ImGuiLayer.h>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Zeyrixon
{
#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

    Application* Application::s_Instance = nullptr;

    Application::Application(const ApplicationCommandLineArgs& args)
        : m_CommandLineArgs(args)
    {
        s_Instance = this;

        m_Window = std::shared_ptr<Window>(Window::Create());
        m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);
    }

    Application::~Application()
    {
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* overlay)
    {
        m_LayerStack.PushOverlay(overlay);
        overlay->OnAttach();
    }

    void Application::Run()
    {
        while (m_Running)
        {
            glClearColor(1, 1, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            for (Layer* layer : m_LayerStack)
                layer->OnUpdate();

            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();

            m_Window->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));

        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
        {
            (*--it)->OnEvent(e);
            if (e.Handled)
                break;
        }
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