#include <pch.h>

#include <Platform/OpenGL/OpenGLVertexArray.h>
#include <Zeyrixon/Core/Log.h>

#include <glad/glad.h>

namespace Zeyrixon
{
    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return GL_FLOAT;
            case ShaderDataType::Float2: return GL_FLOAT;
            case ShaderDataType::Float3: return GL_FLOAT;
            case ShaderDataType::Float4: return GL_FLOAT;
            case ShaderDataType::Mat3:   return GL_FLOAT;
            case ShaderDataType::Mat4:   return GL_FLOAT;
            case ShaderDataType::Int:    return GL_INT;
            case ShaderDataType::Int2:   return GL_INT;
            case ShaderDataType::Int3:   return GL_INT;
            case ShaderDataType::Int4:   return GL_INT;
            case ShaderDataType::Bool:   return GL_BOOL;
        }

        Z_CORE_CRITICAL("Unknown ShaderDataType!");
        return 0;
    }

    OpenGLVertexArray::OpenGLVertexArray()
    {
        glGenVertexArrays(1, &m_RendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(m_RendererID);
    }

    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<OpenGLVertexBuffer>& vertexBuffer)
    {
        if (vertexBuffer->GetLayout().GetElements().empty())
        {
            Z_CORE_CRITICAL("Vertex Buffer has no layout set before being added to a Vertex Array!");
            return;
        }

        glBindVertexArray(m_RendererID);
        vertexBuffer->Bind();

        const auto& layout = vertexBuffer->GetLayout();
        for (const auto& element : layout)
        {
            switch (element.Type)
            {
                case ShaderDataType::Float:
                case ShaderDataType::Float2:
                case ShaderDataType::Float3:
                case ShaderDataType::Float4:
                case ShaderDataType::Int:
                case ShaderDataType::Int2:
                case ShaderDataType::Int3:
                case ShaderDataType::Int4:
                case ShaderDataType::Bool:
                {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(
                        m_VertexBufferIndex,
                        element.GetComponentCount(),
                        ShaderDataTypeToOpenGLBaseType(element.Type),
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        (const void*)element.Offset);
                    m_VertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Mat3:
                case ShaderDataType::Mat4:
                {
                    // A mat3/mat4 attribute needs one vertex attribute slot per column
                    // (a GL attribute can hold at most 4 floats) plus glVertexAttribDivisor
                    // if it's meant for instanced draws. Not needed for basic per-vertex
                    // attributes (position/normal/uv/color), so leaving unimplemented until
                    // there's an actual instanced-rendering use case to get the divisor right.
                    Z_CORE_CRITICAL("Mat3/Mat4 vertex attributes aren't implemented yet!");
                    break;
                }
                default:
                    Z_CORE_CRITICAL("Unknown ShaderDataType in vertex buffer layout!");
                    break;
            }
        }

        m_VertexBuffers.push_back(vertexBuffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<OpenGLIndexBuffer>& indexBuffer)
    {
        glBindVertexArray(m_RendererID);
        indexBuffer->Bind();

        m_IndexBuffer = indexBuffer;
    }
}