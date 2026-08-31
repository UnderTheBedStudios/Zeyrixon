#pragma once

#include <Zeyrixon/Core.h>
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
    private:
        std::unique_ptr<Window> m_Window;
        bool m_Running = true;
    };

    /* Will be defined in client or else nothing will work */
    Application* CreateApplication();
}