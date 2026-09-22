#include <pch.h>

#include <Platform/OpenGL/OpenGLRender.h>

#include <glad/glad.h>

namespace Zeyrixon
{
    void OpenGLRender::Init()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRender::SetClearColor(float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
    }

    void OpenGLRender::SetClearColor(glm::vec4 color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRender::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRender::OnWindowResize(uint32_t width, uint32_t height)
    {
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    }

    void OpenGLRender::DrawIndexed(const std::shared_ptr<OpenGLVertexArray>& vertexArray, uint32_t indexCount)
    {
        vertexArray->Bind();
        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
    }
}