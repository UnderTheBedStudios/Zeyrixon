#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <string>

namespace Zeyrixon
{
    struct IDComponent
    {
        uint64_t ID = 0;

        IDComponent() = default;
        IDComponent(uint64_t id) : ID(id) {}
    };

    struct TagComponent
    {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Rotation{ 0.0f };   // radians
        glm::vec3 Scale{ 1.0f };

        /* T * Rz * Ry * Rx * S. The rotation order matches ImGuizmo's, so a mesh
           drawn with this matrix lines up with the gizmo. */
        glm::mat4 GetMatrix() const
        {
            const glm::mat4 identity(1.f);
            return glm::translate(identity, Position)
                * glm::rotate(identity, Rotation.z, glm::vec3(0.f, 0.f, 1.f))
                * glm::rotate(identity, Rotation.y, glm::vec3(0.f, 1.f, 0.f))
                * glm::rotate(identity, Rotation.x, glm::vec3(1.f, 0.f, 0.f))
                * glm::scale(identity, Scale);
        }

        /* The view matrix of a camera placed at this transform: position and rotation only
           (scale is ignored). A camera looks down its own -Z axis. */
        glm::mat4 GetViewMatrix() const
        {
            const glm::mat4 identity(1.f);
            const glm::mat4 cameraToWorld = glm::translate(identity, Position)
                * glm::rotate(identity, Rotation.z, glm::vec3(0.f, 0.f, 1.f))
                * glm::rotate(identity, Rotation.y, glm::vec3(0.f, 1.f, 0.f))
                * glm::rotate(identity, Rotation.x, glm::vec3(1.f, 0.f, 0.f));
            return glm::inverse(cameraToWorld);
        }
    };

    struct CameraComponent
    {
        bool Orthographic = false;
        float FOV = 45.f;               // degrees; perspective only
        float OrthographicSize = 5.f;   // half the visible height in world units; orthographic only
        float Near = 0.1f;
        float Far = 1000.f;
        bool Primary = true;

        glm::mat4 GetProjection(float aspect) const
        {
            if (Orthographic)
            {
                const float halfHeight = OrthographicSize;
                const float halfWidth = halfHeight * aspect;
                return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, Near, Far);
            }

            return glm::perspective(glm::radians(FOV), aspect, Near, Far);
        }
    };

    struct MeshComponent
    {
        // "builtin:cube", or the path of an .obj file.
        std::string MeshPath = "builtin:cube";
    };

    struct SpriteComponent
    {
        std::string TexturePath;            // empty = a flat coloured square
        glm::vec4 Color{ 1.0f };            // tint; alpha is the opacity
        float PixelsPerUnit = 100.0f;       // texture pixels per world unit, before the entity's scale
        bool FlipX = false;
        bool FlipY = false;
        bool PixelArt = false;              // sharp pixels instead of smoothed ones
        int SortingOrder = 0;               // higher draws on top
    };

    struct MaterialComponent
    {
        glm::vec3 Albedo{ 1.f };    // Tints the albedo map, or the color if there is none
        float Metallic = 0.f;       // Used only when there is no metallic map
        float Roughness = 0.5f;     // Used only when there is no roughness map
        float AO = 1.f;             // Used only when there is no AO map

        // Empty means not assigned. Full file paths for now
        std::string AlbedoMap;
        std::string NormalMap;
        std::string RoughnessMap;
        std::string MetallicMap;
        std::string AOMap;

        bool ApplyMTL(const std::string& mtlPath)
        {
            std::ifstream file(mtlPath);
            if (!file)
                return false;

            const std::filesystem::path directory = std::filesystem::path(mtlPath).parent_path();

            glm::vec3 diffuse(0.8f);
            bool inMaterial = false;

            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream stream(line);
                std::string keyword;
                stream >> keyword;

                if (keyword == "newmtl")
                {
                    if (inMaterial)
                        break;
                    inMaterial = true;
                }
                else if (keyword == "Kd")
                {
                    stream >> diffuse.x >> diffuse.y >> diffuse.z;
                }
                else if (keyword == "map_Kd" || keyword == "map_Pr" || keyword == "map_Pm"
                    || keyword == "map_Bump" || keyword == "bump" || keyword == "norm")
                {
                    // Options such as "-bm 1.0" can come first; the file name is the last token.
                    std::string token, last;
                    while (stream >> token)
                        last = token;
                    if (last.empty())
                        continue;

                    const std::string fullPath = (directory / last).string();
                    if (keyword == "map_Kd")      AlbedoMap = fullPath;
                    else if (keyword == "map_Pr") RoughnessMap = fullPath;
                    else if (keyword == "map_Pm") MetallicMap = fullPath;
                    else                          NormalMap = fullPath;
                }
            }

            // The albedo map carries the colour, so don't also tint it with Kd.
            Albedo = AlbedoMap.empty() ? diffuse : glm::vec3(1.0f);
            return true;
        }
    };
}
