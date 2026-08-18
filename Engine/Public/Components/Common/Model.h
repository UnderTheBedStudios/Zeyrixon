#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <Engine/Public/Components/Common/Mesh.h>

class Model {
public:
    std::vector<Mesh> Meshes;

    bool LoadFromFile(const std::string& path);
    void Draw() const;

    // Path last passed to a *successful* LoadFromFile call. Empty if nothing has loaded yet.
    const std::string& GetPath() const { return m_Path; }

private:
    std::string m_Directory;
    std::string m_Path;

    void ProcessNode(aiNode* node, const aiScene* scene);

    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    unsigned int LoadMaterialTexture(aiMaterial* material, aiTextureType type);
};