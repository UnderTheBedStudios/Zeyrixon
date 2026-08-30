#pragma once

#include <Zeyrixon/Core.h>

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
        bool m_Running = true;
    };

    /* Will be defined in client or else nothing will work */
    Application* CreateApplication();
}