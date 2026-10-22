#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Renderer/Mesh.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Zeyrixon
{
    /* Everything read out of a .gltf / .glb, still as plain CPU data (no OpenGL objects yet).
       Kept apart from Mesh so the file reading can be tested without a GL context. */
    struct GLTFImportResult
    {
        std::vector<MeshVertex> Vertices;
        std::vector<SkinVertex> SkinVertices;       // empty unless something in the file is skinned
        std::vector<uint32_t> Indices;
        std::vector<Submesh> Submeshes;             // one per glTF primitive
        std::vector<MaterialSlot> Materials;        // one per submesh, SubmeshIndex filled in
        std::shared_ptr<ModelData> Model;

        // Images packed inside the file, as (TextureLibrary key, still-encoded bytes).
        std::vector<std::pair<std::string, std::vector<unsigned char>>> EmbeddedImages;
    };

    class Z_API GLTFLoader
    {
    public:
        // Reads the file and builds a GPU mesh from it. Returns null (and logs why) on failure.
        static std::shared_ptr<Mesh> Load(const std::string& path);

        // The CPU-only half of Load.
        static bool Import(const std::string& path, GLTFImportResult& out);
    };
}
