#pragma once

#include <Zeyrixon/Window.h>

namespace Zeyrixon 
{
    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& props);
        virtual ~WindowsWindow();

        void OnUpdate() override;

        inline unsigned int GetWidth() const override { return m_Data.Width; }
        inline unsigned int GetHeight() const override { return m_Data.Height; }

        GLFWwindow* GetWindow() override { return m_Window; }

    private:
        virtual void Init(const WindowProps& props);
        virtual void Shutdown();
        
        GLFWwindow* m_Window;
        struct WindowData
        {
            std::string Title;
            unsigned int Width, Height;
        };

        WindowData m_Data;
    };
}
