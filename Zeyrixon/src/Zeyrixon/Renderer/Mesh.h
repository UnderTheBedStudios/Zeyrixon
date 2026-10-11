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

        // A CPU-side copy of the geometry, kept for picking, plus it's bounding box in the mesh's own space.
        const std::vector<MeshVertex>& GetVerticies() const { return m_Verticies; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
        const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

    private:
        std::shared_ptr<OpenGLVertexArray> m_VertexArray;
        std::vector<MeshVertex> m_Verticies;
        std::vector<uint32_t> m_Indices;
        glm::vec3 m_BoundsMin{ 0.f };
        glm::vec3 m_BoundsMax{ 0.f };
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
