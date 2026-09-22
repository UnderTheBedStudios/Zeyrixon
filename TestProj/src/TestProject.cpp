#include <Zeyrixon.h>

#include <Platform/OpenGL/OpenGLRender.h>
#include <Platform/OpenGL/OpenGLVertexArray.h>
#include <Platform/OpenGL/OpenGLBuffer.h>
#include <Platform/OpenGL/OpenGLShader.h>
#include <Platform/OpenGL/OpenGLUniformBuffer.h>

#include <glm/gtc/matrix_transform.hpp>

class TestLayer : public Zeyrixon::Layer
{
public:
    TestLayer()
        : Layer("Test")
    {
    }

    void OnAttach() override
    {
        float vertices[3 * 3] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };
        uint32_t indices[3] = { 0, 1, 2 };

        m_VertexArray = std::make_shared<Zeyrixon::OpenGLVertexArray>();

        auto vertexBuffer = std::make_shared<Zeyrixon::OpenGLVertexBuffer>(vertices, sizeof(vertices));
        vertexBuffer->SetLayout({
            { Zeyrixon::ShaderDataType::Float3, "a_Position" }
        });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        auto indexBuffer = std::make_shared<Zeyrixon::OpenGLIndexBuffer>(indices, 3);
        m_VertexArray->SetIndexBuffer(indexBuffer);

        std::string vertexSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Position;

            layout(std140, binding = 0) uniform CameraData
            {
                mat4 u_ViewProjection;
            };
            
            void main()
            {
                gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
            }
        )";

        std::string fragmentSrc = R"(
            #version 450 core
            out vec4 color;
            void main()
            {
                color = vec4(0.8, 0.3, 0.2, 1.0);
            }
        )";

        m_Shader = std::make_shared<Zeyrixon::OpenGLShader>("Test", vertexSrc, fragmentSrc);

        m_CameraUniformBuffer = std::make_shared<Zeyrixon::OpenGLUniformBuffer>(sizeof(glm::mat4), 0);
    }

    void OnUpdate() override
    {
        if (Zeyrixon::Input::IsKeyPressed(Z_KEY_TAB))
            Z_INFO("Tab key was pressed!");

        if (Zeyrixon::Input::IsMouseButtonPressed(Z_MOUSE_BUTTON_LEFT))
            Z_INFO("Left Mouse Button was pressed!");

        auto window = Zeyrixon::Application::Get().GetWindow();
        float aspect = (float)window->GetWidth() / (float)window->GetHeight();
        glm::mat4 viewProjection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        m_CameraUniformBuffer->SetData(&viewProjection, sizeof(glm::mat4));

        m_Shader->Bind();
        Zeyrixon::OpenGLRender::DrawIndexed(m_VertexArray);
    }

private:
    std::shared_ptr<Zeyrixon::OpenGLVertexArray> m_VertexArray;
    std::shared_ptr<Zeyrixon::OpenGLShader> m_Shader;
    std::shared_ptr<Zeyrixon::OpenGLUniformBuffer> m_CameraUniformBuffer;
};

namespace
{
    /* It inherits from the Application so that it can have the same functions and yah */
    class TestProject : public Zeyrixon::Application
    {
    public:
        TestProject()
        {
            PushLayer(new TestLayer());
        }

        ~TestProject()
        {

        }
    };
}

/* This just makes it so that your app exists :) */
Zeyrixon::Application* Zeyrixon::CreateApplication(Zeyrixon::ApplicationCommandLineArgs args)
{
    TestProject* project = new TestProject();
    project->ChangeWindowImage("Icon.png");
    project->ChangeWindowTitle("Test Project");
    return project;
}