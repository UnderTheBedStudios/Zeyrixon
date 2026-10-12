#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    };
}
