#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Renderer/ModelData.h>
#include <Zeyrixon/World/Components.h>
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

    /* Extra per-vertex data for skinned (bone-animated) meshes, kept in its own vertex buffer so
       meshes without a skeleton don't pay for it. Stored as floats because the engine's vertex
       layouts have no integer attributes; the joint indices are whole numbers in 0..127. */
    struct SkinVertex
    {
        glm::vec4 Joints{ 0.0f };    // indices into the skin's joint list
        glm::vec4 Weights{ 0.0f };   // how much each of those joints pulls on the vertex; sums to 1
    };

    /* A contiguous slice of a mesh's index buffer that uses one material. Comes from the OBJ's
       "usemtl" lines, or from one glTF primitive; a mesh without any is a single submesh covering
       everything. */
    struct Submesh
    {
        std::string MaterialName;   // the usemtl / glTF material name; empty = none given
        uint32_t IndexOffset = 0;   // first index, counted in indices (not bytes)
        uint32_t IndexCount = 0;

        /* glTF only. NodeIndex: the model node this part is attached to, but only if an animation
           moves that node (otherwise -1: the vertices already sit where they belong). SkinIndex: the
           skeleton that deforms it, or -1 if it isn't skinned. */
        int NodeIndex = -1;
        int SkinIndex = -1;
    };

    class Z_API Mesh
    {
    public:
        /* skinVertices is empty for a mesh with no skeleton, otherwise one per vertex. model and
           materials come from glTF files: the hierarchy/skeleton/animations, and one material per
           submesh (MaterialSlot::SubmeshIndex says which). */
        Mesh(std::vector<MeshVertex> verticies, std::vector<uint32_t> indices, std::vector<Submesh> submeshes = {},
             std::vector<SkinVertex> skinVertices = {}, std::shared_ptr<ModelData> model = nullptr,
             std::vector<MaterialSlot> materials = {});

        const std::shared_ptr<OpenGLVertexArray>& GetVertexArray() const { return m_VertexArray; }

        // A CPU-side copy of the geometry, kept for picking, plus it's bounding box in the mesh's own space.
        // (For skinned glTF parts the copy is in the model's rest pose, i.e. where the part is drawn without animation.)
        const std::vector<MeshVertex>& GetVerticies() const { return m_Verticies; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
        const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }
        const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

        // Null unless the mesh came from a glTF file. Holds the skeleton and the animations.
        const std::shared_ptr<ModelData>& GetModel() const { return m_Model; }

        // The materials the file itself defines (glTF only). Used when the entity has no Material component.
        const std::vector<MaterialSlot>& GetImportedMaterials() const { return m_ImportedMaterials; }

    private:
        std::shared_ptr<OpenGLVertexArray> m_VertexArray;
        std::vector<MeshVertex> m_Verticies;
        std::vector<uint32_t> m_Indices;
        std::vector<Submesh> m_Submeshes;
        std::shared_ptr<ModelData> m_Model;
        std::vector<MaterialSlot> m_ImportedMaterials;
        glm::vec3 m_BoundsMin{ 0.f };
        glm::vec3 m_BoundsMax{ 0.f };
    };

    /* Loads each mesh once and hands out shared copies. Paths:
            "builtin:cube" a unit cube generated in code
            *.gltf / *.glb a glTF model (can carry a skeleton, animations and materials)
            anything else  an .obj file on disk
        A path that fails to load is remembered as failed (and logged once)
        rather than retried every frame.*/
    class Z_API MeshLibrary
    {
    public:
        static std::shared_ptr<Mesh> Get(const std::string& path);
    };
}
