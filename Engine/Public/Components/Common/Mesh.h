#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm.hpp>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class Mesh
{
public:
    Mesh& operator=(Mesh&& other) noexcept;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, unsigned int textureID);
    ~Mesh();

    Mesh(Mesh&& other) noexcept;

    void Draw() const;

private:
    unsigned int m_VAO = 0, m_VBO = 0, m_EBO = 0;
    unsigned int m_TextureID = 0;
    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    void SetupMesh();
};