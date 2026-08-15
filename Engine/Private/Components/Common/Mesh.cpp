#include <Engine/Public/Components/Common/Mesh.h>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, unsigned int textureID)
{
    m_TextureID = textureID;
    m_Vertices = std::move(vertices);
    m_Indices = std::move(indices);
    SetupMesh();
}

Mesh::~Mesh()
{
    // Guard against double-free if a Mesh is ever left in a moved-from state
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);
}

Mesh::Mesh(Mesh&& other) noexcept
{
    m_VAO = other.m_VAO;
    m_VBO = other.m_VBO;
    m_EBO = other.m_EBO;

    m_TextureID = other.m_TextureID;

    m_Vertices = other.m_Vertices;
    m_Indices = other.m_Indices;

    other.m_VAO = other.m_VBO = other.m_EBO = other.m_TextureID = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
        if (m_VBO) glDeleteBuffers(1, &m_VBO);
        if (m_EBO) glDeleteBuffers(1, &m_EBO);

        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_TextureID = other.m_TextureID;
        m_Vertices = std::move(other.m_Vertices);
        m_Indices = std::move(other.m_Indices);

        other.m_VAO = other.m_VBO = other.m_EBO = other.m_TextureID = 0;
    }
    return *this;
}

void Mesh::SetupMesh()
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 m_Vertices.size() * sizeof(Vertex),
                 m_Vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 m_Indices.size() * sizeof(unsigned int),
                 m_Indices.data(),
                 GL_STATIC_DRAW);

    // Position -> location 0 (matches basic.vert's aPos)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                           sizeof(Vertex),
                           (void*)offsetof(Vertex, Position));

    // Normal -> location 1 (matches basic.vert's aNormal)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                            sizeof(Vertex),
                            (void*)offsetof(Vertex, Normal));
    // TexCoords -> location 2 (matches basic.vert's aTexCoord)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                           sizeof(Vertex),
                           (void*)offsetof(Vertex, TexCoords));


    glBindVertexArray(0);
}

void Mesh::Draw() const
{
    if (m_TextureID)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);
    }

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(m_Indices.size()),
                   GL_UNSIGNED_INT,
                   0);
    glBindVertexArray(0);
}