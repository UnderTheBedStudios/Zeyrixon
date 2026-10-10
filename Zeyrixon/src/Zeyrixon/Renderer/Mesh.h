#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Platform/OpenGL/OpenGLVertexArray.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Zeyrixon
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    class Z_API Mesh
    {
    public:
        Mesh(std::vector<MeshVertex> verticies, std::vector<uint32_t> indices);

        const std::shared_ptr<OpenGLVertexArray>& GetVertexArray() const { return m_VertexArray; }

    private:
        std::shared_ptr<OpenGLVertexArray> m_VertexArray;
    };

    /* Loads each mesh once and hands out shared copies. Paths:
            "builtin:cube" a unit cube generated in code
            anything else  an .obj file on disk
        A path that fails to load is remembered as failed (and logged once)
        rather than retried every frame.*/
    class Z_API MeshLibrary
    {
    public:
        static std::shared_ptr<Mesh> Get(const std::string& path);
    };
}
