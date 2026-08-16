#include <Editor/Public/Common/Window.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>

Window::Window(const WindowDesc& desc) : m_type(desc.type) 
{
    InitGLFW(desc);
    InitImGui();
}

Window::~Window()
{
    MakeCurrent();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imguiContext);
    glfwDestroyWindow(m_handle);
}

void Window::InitGLFW(const WindowDesc& desc)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, desc.decorated ? GLFW_TRUE : GLFW_FALSE);

    if (desc.type == WindowType::Dialog)
    {
        if (!desc.parent)
            throw std::runtime_error("Dialog window requires parent GLFWwindow*");
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    }
    else
        glfwWindowHint(GLFW_FLOATING, desc.alwaysOnTop ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* share = (desc.type == WindowType::Dialog) ? desc.parent : nullptr;
    m_handle = glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), nullptr, share);
    if (!m_handle)
        throw std::runtime_error("glfwCreateWindow failed for \"" + desc.title + "\"");
    
    if (desc.type == WindowType::Dialog && desc.centerOnParent)
    {
#if defined(GLFW_VERSION_MAJOR) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
        if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
#endif
        {
            int px, py, pw, ph;
            glfwGetWindowPos(desc.parent, &px, &py);
            glfwGetWindowSize(desc.parent, &pw, &ph);
            glfwSetWindowPos(m_handle, px + (pw - desc.width) / 2, py + (ph - desc.height) / 2);
        }
    }
}

void Window::InitImGui()
{
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    glfwMakeContextCurrent(m_handle);
    ImGui_ImplGlfw_InitForOpenGL(m_handle, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");
}

void Window::MakeCurrent()
{
    glfwMakeContextCurrent(m_handle);
    ImGui::SetCurrentContext(m_imguiContext);
}

void Window::BeginFrame()
{
    MakeCurrent();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::EndFrame()
{
    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(m_handle, &w, &h);
    glViewport(0, 0, w, h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_handle);
}