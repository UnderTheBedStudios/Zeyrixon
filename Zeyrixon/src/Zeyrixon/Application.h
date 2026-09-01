#pragma once

#include <Zeyrixon/Core.h>
#include <Zeyrixon/Events/Event.h>
#include <Zeyrixon/LayerStack.h>
#include <Zeyrixon/Events/ApplicationEvent.h>
#include <Zeyrixon/Window.h>

namespace Zeyrixon
{
    class Z_API Application
    {
    public:
        Application();
        virtual ~Application();

        /* This is meant to make the app go vroom vroom :) */
        void Run();

        void OnEvent(Event& e);

        //--------------- Window Stuff ---------------

        std::shared_ptr<Window> GetWindow() { return m_Window; }
        void ChangeWindowImage(const char* path);

        void ChangeWindowTitle(const char* name);

        //--------------- Layer Stuff ---------------

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

    private:
        bool OnWindowClose(WindowCloseEvent& e);

        std::shared_ptr<Window> m_Window;
        bool m_Running = true;
        LayerStack m_LayerStack;
    };

    /* Will be defined in client or else nothing will work */
    Application* CreateApplication();
}