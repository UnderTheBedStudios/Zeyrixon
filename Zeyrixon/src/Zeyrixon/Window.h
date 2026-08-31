#pragma once

#include <pch.h>

#include <Zeyrixon/Core.h>
#include <GLFW/glfw3.h>

namespace Zeyrixon
{
    /* This struct will only be used for when you make a window or you're currently modifying a window */
    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(const std::string& title = "Zeyrixon Engine",
                    unsigned int width = 1280,
                    unsigned int height = 720)
            : Title(title), Width(width), Height(height)
        {}
    };

    // Interface representing a desktop system based window
    class Z_API Window
    {
    public:
        virtual ~Window() {}

        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        virtual GLFWwindow* GetWindow() = 0;

        static Window* Create(const WindowProps& props = WindowProps());
    };
}