#pragma once

#include <Zeyrixon/Core/Core.h>

struct GLFWwindow;

namespace Zeyrixon
{
    class Z_API OpenGLContext
    {
    public:
        OpenGLContext(GLFWwindow* windowHandle);
        void Init();
        void SwapBuffers();

    private:
        GLFWwindow* m_WindowHandle;
    };
}