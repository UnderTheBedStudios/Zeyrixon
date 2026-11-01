#include <pch.h>

#include <Zeyrixon/Renderer/DebugRenderer.h>
#include <Platform/OpenGL/OpenGLShader.h>

#include <glad/glad.h>

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <vector>

namespace Zeyrixon
{
    namespace
    {
        struct LineVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        std::vector<LineVertex> s_Lines;   // two vertices per line

        const char* k_VertexSource = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec4 a_Color;

            uniform mat4 u_ViewProjection;

            out vec4 v_Color;

            void main()
            {
                v_Color = a_Color;
                gl_Position = u_ViewProjection * vec4(a_Position, 1.0);

                // A wireframe that lies exactly ON a surface (a box collider around a cube mesh) would
                // flicker against it, because the line and the surface have the same depth. Pulling
                // the line a hair towards the camera settles that.
                gl_Position.z -= 0.0002 * gl_Position.w;
            }
        )";

        const char* k_FragmentSource = R"(
            #version 450 core
            in vec4 v_Color;
            uniform float u_AlphaScale;
            out vec4 o_Color;

            void main()
            {
                o_Color = vec4(v_Color.rgb, v_Color.a * u_AlphaScale);
            }
        )";

        OpenGLShader& GetShader()
        {
            // Never freed, for the same reason as the other GL statics: it would be destroyed after the
            // GL context is already gone.
            static OpenGLShader* shader = new OpenGLShader("DebugLines", k_VertexSource, k_FragmentSource);
            return *shader;
        }

        struct LineBuffers
        {
            GLuint VertexArray = 0;
            GLuint VertexBuffer = 0;
        };

        LineBuffers& GetBuffers()
        {
            static LineBuffers* buffers = []
            {
                auto* b = new LineBuffers();
                glGenVertexArrays(1, &b->VertexArray);
                glGenBuffers(1, &b->VertexBuffer);

                glBindVertexArray(b->VertexArray);
                glBindBuffer(GL_ARRAY_BUFFER, b->VertexBuffer);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (const void*)offsetof(LineVertex, Position));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (const void*)offsetof(LineVertex, Color));

                glBindVertexArray(0);
                return b;
            }();
            return *buffers;
        }
    }

    void DebugRenderer::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
    {
        s_Lines.push_back({ from, color });
        s_Lines.push_back({ to, color });
    }

    void DebugRenderer::DrawCircle(const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB,
                                   float radius, const glm::vec4& color, int segments)
    {
        glm::vec3 previous = center + axisA * radius;
        for (int i = 1; i <= segments; ++i)
        {
            const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
            const glm::vec3 point = center + (axisA * std::cos(angle) + axisB * std::sin(angle)) * radius;
            DrawLine(previous, point, color);
            previous = point;
        }
    }

    void DebugRenderer::Flush(const glm::mat4& view, const glm::mat4& projection, bool seeThrough)
    {
        if (s_Lines.empty())
            return;

        LineBuffers& buffers = GetBuffers();
        OpenGLShader& shader = GetShader();

        // Streaming upload: the queue changes every frame, so the buffer is simply refilled.
        glBindVertexArray(buffers.VertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, buffers.VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s_Lines.size() * sizeof(LineVertex)), s_Lines.data(), GL_STREAM_DRAW);

        shader.Bind();
        shader.SetMat4("u_ViewProjection", projection * view);

        // Same blending as the grid and sprites: the separate alpha function keeps the framebuffer's own
        // alpha at 1, so ImGui doesn't draw the viewport see-through where a line was drawn.
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDepthMask(GL_FALSE);   // lines never hide anything

        const GLsizei count = static_cast<GLsizei>(s_Lines.size());

        if (seeThrough)
        {
            glDisable(GL_DEPTH_TEST);
            shader.SetFloat("u_AlphaScale", 0.25f);
            glDrawArrays(GL_LINES, 0, count);
        }

        glEnable(GL_DEPTH_TEST);
        shader.SetFloat("u_AlphaScale", 1.0f);
        glDrawArrays(GL_LINES, 0, count);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glBindVertexArray(0);

        s_Lines.clear();
    }
}
