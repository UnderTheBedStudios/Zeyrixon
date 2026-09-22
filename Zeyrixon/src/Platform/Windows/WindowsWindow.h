#pragma once

#include <Zeyrixon/Core/Window.h>
#include <Platform/OpenGL/OpenGLContext.h>

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

        // Window attributes
        inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void SetVSync(bool enabled);
        bool IsVSync() const;

        inline virtual void* GetNativeWindow() const { return m_Window; }

    private:
        virtual void Init(const WindowProps& props);
        virtual void Shutdown();
        
        GLFWwindow* m_Window;
        std::unique_ptr<OpenGLContext> m_Context;
        
        struct WindowData
        {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;

            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
    };
}
