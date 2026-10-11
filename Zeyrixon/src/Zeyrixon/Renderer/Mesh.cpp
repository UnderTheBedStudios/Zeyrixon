#include <pch.h>

#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/Core/Log.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace Zeyrixon
{
    static_assert(sizeof(MeshVertex) == 8 * sizeof(float), "MeshVertex must be tightly packed");

    Mesh::Mesh(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices)
        : m_Verticies(std::move(vertices)), m_Indices(std::move(indices))
    {
        if (!m_Verticies.empty())
        {
            m_BoundsMin = m_BoundsMax = m_Verticies[0].Position;
            for (const MeshVertex& vertex : m_Verticies)
            {
                m_BoundsMin = glm::min(m_BoundsMin, vertex.Position);
                m_BoundsMax = glm::max(m_BoundsMax, vertex.Position);
            }
        }

        m_VertexArray = std::make_shared<OpenGLVertexArray>();

        auto vertexBuffer = std::make_shared<OpenGLVertexBuffer>(
            reinterpret_cast<float*>(m_Verticies.data()),
            static_cast<uint32_t>(m_Verticies.size() * sizeof(MeshVertex)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" }
        });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        auto indexBuffer = std::make_shared<OpenGLIndexBuffer>(
            m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
        m_VertexArray->SetIndexBuffer(indexBuffer);
    }

    namespace
    {
        /* Deliberately never destroyed: its destructor would free GL objects during static
           teardown, after the GL context is already gone. */
        std::unordered_map<std::string, std::shared_ptr<Mesh>>& Cache()
        {
            static auto* cache = new std::unordered_map<std::string, std::shared_ptr<Mesh>>();
            return *cache;
        }

        std::shared_ptr<Mesh> CreateCube()
        {
            // Each face: its outward normal, plus right x up == normal so winding is counter-clockwise.
            struct Face { glm::vec3 Normal, Right, Up; };
            const Face faces[6] = {
                { {  0, 0,  1 }, {  1, 0,  0 }, { 0, 1,  0 } },   // +Z
                { {  0, 0, -1 }, { -1, 0,  0 }, { 0, 1,  0 } },   // -Z
                { {  1, 0,  0 }, {  0, 0, -1 }, { 0, 1,  0 } },   // +X
                { { -1, 0,  0 }, {  0, 0,  1 }, { 0, 1,  0 } },   // -X
                { {  0, 1,  0 }, {  1, 0,  0 }, { 0, 0, -1 } },   // +Y
                { {  0, -1, 0 }, {  1, 0,  0 }, { 0, 0,  1 } },   // -Y
            };
            const glm::vec2 signs[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
            const glm::vec2 uvs[4]   = {  {  0,  0 }, { 1,  0 }, { 1, 1 }, {  0, 1 } };

            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> indices;

            for (const Face& face : faces)
            {
                const uint32_t base = static_cast<uint32_t>(vertices.size());
                for (int i = 0; i < 4; ++i)
                {
                    MeshVertex vertex;
                    vertex.Position = face.Normal * 0.5f + (face.Right * signs[i].x + face.Up * signs[i].y) * 0.5f;
                    vertex.Normal = face.Normal;
                    vertex.TexCoord = uvs[i];
                    vertices.push_back(vertex);
                }
                indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
            }

            return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        }

        // Splits "v", "v/vt", "v//vn" or "v/vt/vn" into OBJ indices (0 = missing).
        void ParseFaceVertex(const std::string& token, int& v, int& vt, int& vn)
        {
            v = vt = vn = 0;

            if (token.find("//") != std::string::npos)
            {
                std::sscanf(token.c_str(), "%d//%d", &v, &vn);
                return;
            }

            const auto slashes = std::count(token.begin(), token.end(), '/');
            if (slashes == 0)      std::sscanf(token.c_str(), "%d", &v);
            else if (slashes == 1) std::sscanf(token.c_str(), "%d/%d", &v, &vt);
            else                   std::sscanf(token.c_str(), "%d/%d/%d", &v, &vt, &vn);
        }

        // OBJ indices are 1-based, and negative values count back from the end of the list so far.
        int ResolveIndex(int index, size_t count)
        {
            if (index > 0) return index - 1;
            if (index < 0) return static_cast<int>(count) + index;
            return -1;
        }

        std::shared_ptr<Mesh> LoadOBJ(const std::string& path)
        {
            std::ifstream file(path);
            if (!file)
            {
                Z_CORE_ERROR("Could not open mesh file '{0}'", path);
                return nullptr;
            }

            std::vector<glm::vec3> positions, normals;
            std::vector<glm::vec2> texCoords;

            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> indices;
            std::map<std::tuple<int, int, int>, uint32_t> uniqueVertices;

            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream stream(line);
                std::string keyword;
                stream >> keyword;

                if (keyword == "v")
                {
                    glm::vec3 p;
                    stream >> p.x >> p.y >> p.z;
                    positions.push_back(p);
                }
                else if (keyword == "vn")
                {
                    glm::vec3 n;
                    stream >> n.x >> n.y >> n.z;
                    normals.push_back(n);
                }
                else if (keyword == "vt")
                {
                    glm::vec2 t;
                    stream >> t.x >> t.y;
                    texCoords.push_back(t);
                }
                else if (keyword == "f")
                {
                    // The face's corners as (position, uv, normal) indices.
                    std::vector<std::tuple<int, int, int>> corners;
                    std::string token;
                    while (stream >> token)
                    {
                        int v, vt, vn;
                        ParseFaceVertex(token, v, vt, vn);
                        corners.emplace_back(
                            ResolveIndex(v, positions.size()),
                            ResolveIndex(vt, texCoords.size()),
                            ResolveIndex(vn, normals.size()));
                    }

                    // Triangulate as a fan: (0,1,2), (0,2,3), ...
                    for (size_t i = 1; i + 1 < corners.size(); ++i)
                    {
                        const std::tuple<int, int, int>* triangle[3] = { &corners[0], &corners[i], &corners[i + 1] };

                        bool valid = true;
                        for (const auto* corner : triangle)
                        {
                            const int pi = std::get<0>(*corner);
                            if (pi < 0 || pi >= static_cast<int>(positions.size()))
                                valid = false;
                        }
                        if (!valid)
                            continue;

                        const glm::vec3& p0 = positions[std::get<0>(*triangle[0])];
                        const glm::vec3& p1 = positions[std::get<0>(*triangle[1])];
                        const glm::vec3& p2 = positions[std::get<0>(*triangle[2])];
                        glm::vec3 faceNormal(0.0f, 1.0f, 0.0f);
                        const glm::vec3 crossed = glm::cross(p1 - p0, p2 - p0);
                        if (glm::length(crossed) > 0.0f)
                            faceNormal = glm::normalize(crossed);

                        for (const auto* corner : triangle)
                        {
                            const int pi = std::get<0>(*corner);
                            const int ti = std::get<1>(*corner);
                            const int ni = std::get<2>(*corner);
                            const bool hasNormal = ni >= 0 && ni < static_cast<int>(normals.size());

                            // Corners with the same position, uv and normal are merged. A corner without a
                            // normal gets the face normal, so it can't be shared between faces.
                            if (hasNormal)
                            {
                                auto found = uniqueVertices.find(*corner);
                                if (found != uniqueVertices.end())
                                {
                                    indices.push_back(found->second);
                                    continue;
                                }
                            }

                            MeshVertex vertex;
                            vertex.Position = positions[pi];
                            vertex.Normal = hasNormal ? normals[ni] : faceNormal;
                            vertex.TexCoord = (ti >= 0 && ti < static_cast<int>(texCoords.size())) ? texCoords[ti] : glm::vec2(0.0f);

                            const uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                            vertices.push_back(vertex);
                            indices.push_back(newIndex);
                            if (hasNormal)
                                uniqueVertices[*corner] = newIndex;
                        }
                    }
                }
            }

            if (vertices.empty())
            {
                Z_CORE_ERROR("Mesh file '{0}' contains no triangles", path);
                return nullptr;
            }

            return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        }
    }

    std::shared_ptr<Mesh> MeshLibrary::Get(const std::string& path)
    {
        auto& cache = Cache();

        auto found = cache.find(path);
        if (found != cache.end())
            return found->second;   // may be null: a path that already failed isn't retried or logged again

        std::shared_ptr<Mesh> mesh = (path == "builtin:cube") ? CreateCube() : LoadOBJ(path);
        cache[path] = mesh;
        return mesh;
    }
}
