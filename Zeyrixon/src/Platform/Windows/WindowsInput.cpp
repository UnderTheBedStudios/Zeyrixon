#include <pch.h>

#include <Platform/Windows/WindowsInput.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>

#include <GLFW/glfw3.h>

namespace Zeyrixon
{
    Input* Input::s_Instance = new WindowsInput();

    bool WindowsInput::IsKeyPressedImpl(int keycode)
    {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow()->GetNativeWindow());
        auto state = glfwGetKey(window, keycode);

        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool WindowsInput::IsMouseButtonPressedImpl(int button)
    {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow()->GetNativeWindow());
        auto state = glfwGetMouseButton(window, button);

        return state == GLFW_PRESS;
    }

    std::pair<float, float> WindowsInput::GetMousePosImpl()
    {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow()->GetNativeWindow());
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        return { (float)xpos, (float)ypos };
    }

    float WindowsInput::GetMouseXImpl()
    {
        auto[x,y] = GetMousePosImpl();
        return x;
    }

    float WindowsInput::GetMouseYImpl()
    {
        auto[x,y] = GetMousePosImpl();
        return y;
    }

    void WindowsInput::SetCursorModeImpl(CursorMode mode)
    {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow()->GetNativeWindow());

        int glfwMode = GLFW_CURSOR_NORMAL;
        if (mode == CursorMode::Hidden) glfwMode = GLFW_CURSOR_HIDDEN;
        if (mode == CursorMode::Locked) glfwMode = GLFW_CURSOR_DISABLED;

        glfwSetInputMode(window, GLFW_CURSOR, glfwMode);

        /* Raw motion skips the OS pointer acceleration, which feels better for a camera.
           It only has an effect while the cursor is locked. */
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, mode == CursorMode::Locked ? GLFW_TRUE : GLFW_FALSE);
    }

    CursorMode WindowsInput::GetCursorModeImpl()
    {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow()->GetNativeWindow());

        int glfwMode = glfwGetInputMode(window, GLFW_CURSOR);

        if (glfwMode == GLFW_CURSOR_NORMAL) return CursorMode::Normal;
        if (glfwMode == GLFW_CURSOR_HIDDEN) return CursorMode::Hidden;
        if (glfwMode == GLFW_CURSOR_DISABLED) return CursorMode::Locked;

        Z_CORE_WARN("Couldn't find the Cursor Mode!");

        return CursorMode::Normal;
    }
}
