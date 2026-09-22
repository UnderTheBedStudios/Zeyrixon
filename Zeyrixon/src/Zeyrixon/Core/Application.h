#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Events/Event.h>
#include <Zeyrixon/Core/LayerStack.h>
#include <Zeyrixon/Events/ApplicationEvent.h>
#include <Zeyrixon/Core/Window.h>

namespace Zeyrixon
{
    class ImGuiLayer;

    struct ApplicationCommandLineArgs
    {
        int Count = 0;
        char** Args = nullptr;

        const char* operator[](int index) const
        {
            return Args[index];
        }
    };

    class Z_API Application
    {
    public:
        Application(const ApplicationCommandLineArgs& args = ApplicationCommandLineArgs());
        virtual ~Application();

        const ApplicationCommandLineArgs& GetCommandLineArgs() const { return m_CommandLineArgs; }

        void Run();
        void OnEvent(Event& e);

        std::shared_ptr<Window> GetWindow() { return m_Window; }
        void ChangeWindowImage(const char* path);
        void ChangeWindowTitle(const char* name);

        // --------- Layer Stuff ---------

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

        ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
        static Application& Get() { return *s_Instance; }

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

        std::shared_ptr<Window> m_Window;
        bool m_Running = true;
        LayerStack m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;
        ApplicationCommandLineArgs m_CommandLineArgs;

        static Application* s_Instance;
    };

    // Implemented per-application (e.g. in ZeyrixonEditor/src/main.cpp)
    Application* CreateApplication(ApplicationCommandLineArgs args);
}