#include <Engine/Public/Components/Common/Model.h>
#include <glad/glad.h>
#include <cstdio>
#include <assimp/postprocess.h>

#include <stb_image.h>

bool Model::LoadFromFile(const std::string& path)
{
    Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(stderr, "[Model] Assimp error loading '%s': %s\n", path.c_str(), importer.GetErrorString());
        return false;
    }

    // Everything in the .mtl (e.g. "Textures/CamTex.png") is relative to the .obj's own folder
    size_t lastSlash = path.find_last_of('/');
    m_Directory = (lastSlash == std::string::npos) ? "." : path.substr(0, lastSlash);

    Meshes.clear();
    ProcessNode(scene->mRootNode, scene);
    m_Path = path;
    return true;
}

void Model::Draw() const
{
    for (const auto& mesh : Meshes)
        mesh.Draw();
}

unsigned int Model::LoadMaterialTexture(aiMaterial* material, aiTextureType type)
{
    if (material->GetTextureCount(type) == 0)
        return 0;

    aiString str;
    material->GetTexture(type, 0, &str);
    std::string fullPath = m_Directory + "/" + str.C_Str();

    unsigned int textureID;
    glGenTextures(1, &textureID);

    stbi_set_flip_vertically_on_load(true);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int width, height, channels;
    unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        fprintf(stderr, "[Model] Failed to load texture: %s\n", fullPath.c_str());
        glDeleteTextures(1, &textureID);
        return 0;
    }

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Meshes.push_back(ProcessMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene);
}

Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex v{};
        v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        if (mesh->HasNormals())
            v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
        if (mesh->mTextureCoords[0])
            v.TexCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        vertices.push_back(v);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    unsigned int textureID = 0;
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        // map_Kd in the .mtl -> aiTextureType_DIFFUSE. map_Ns (roughness) and map_refl (metalness)
        // are in Camera.mtl too, but basic.frag has no roughness/metal path yet, so only diffuse is loaded.
        textureID = LoadMaterialTexture(material, aiTextureType_DIFFUSE);
    }

    return Mesh(std::move(vertices), std::move(indices), textureID);
}
