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
    };

    struct CameraComponent
    {
        float FOV = 45.f;
        float Near = 0.1f;
        float Far = 1000.f;
        bool Primary = true;
    };

    struct MeshComponent
    {
        // "builtin:cube", or the path of an .obj file.
        std::string MeshPath = "builtin:cube";
        glm::vec3 Color{ 0.8f };
    };
}
