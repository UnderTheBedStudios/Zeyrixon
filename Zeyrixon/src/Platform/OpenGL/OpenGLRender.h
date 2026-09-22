#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Platform/OpenGL/OpenGLVertexArray.h>

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace Zeyrixon
{
    class Z_API OpenGLRender
    {
    public:
        static void Init();

        static void SetClearColor(float r, float g, float b, float a);
        static void SetClearColor(glm::vec4 rgba);
        static void Clear();
        static void OnWindowResize(uint32_t width, uint32_t height);

        static void DrawIndexed(const std::shared_ptr<OpenGLVertexArray>& vertexArray, uint32_t indexCount = 0);
    };
}