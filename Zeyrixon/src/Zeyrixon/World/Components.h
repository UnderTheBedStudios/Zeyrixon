#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

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

    /* Plays one of the animations of a glTF mesh on the same entity (it does nothing for an OBJ or the
       built-in cube). The clip is chosen by its position in the file's animation list. */
    struct AnimationComponent
    {
        int ClipIndex = 0;          // -1 = none: the model stays in its rest pose
        float Speed = 1.0f;         // 1 = normal; negative plays backwards
        bool Loop = true;
        bool PlayOnStart = true;    // starts playing when Play is pressed

        // Runtime state; not saved. Time is also what the Time slider in the inspector scrubs.
        float Time = 0.0f;          // seconds into the clip
        bool Playing = false;
    };

    struct ScriptComponent
    {
        std::string ScriptPath;

        std::string ClassName;

        std::string GetResolvedClassName() const
        {
            if (!ClassName.empty())
                return ClassName;
            return std::filesystem::path(ScriptPath).stem().string();
        }
    };

    /* One material. A mesh with several materials (an OBJ with several "usemtl") is split into
       submeshes, and each slot says which submesh it is drawn on. */
    struct MaterialSlot
    {
        std::string Name;           // The .mtl material name; used to match submeshes on import
        int SubmeshIndex = -1;      // -1 = every submesh that has no slot of its own

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

        /* glTF stores roughness (green channel) and metallic (blue channel) in ONE texture. When this is
           set, RoughnessMap holds that texture, MetallicMap is ignored, and Roughness / Metallic above
           multiply what the texture says instead of being replaced by it. */
        bool PackedMetallicRoughness = false;
    };

    /* EnTT allows only one component of each type per entity, so the several materials live in
       a list inside the one component. */
    struct MaterialComponent
    {
        std::vector<MaterialSlot> Slots{ MaterialSlot{} };   // a new component starts with one

        // The slot for this submesh: its own, else the first -1 slot, else null.
        const MaterialSlot* FindSlot(int submeshIndex) const
        {
            const MaterialSlot* fallback = nullptr;
            for (const MaterialSlot& slot : Slots)
            {
                if (slot.SubmeshIndex == submeshIndex)
                    return &slot;
                if (slot.SubmeshIndex < 0 && !fallback)
                    fallback = &slot;
            }
            return fallback;
        }

        /* Reads every material in the .mtl and replaces the slots with the ones the mesh uses.
           submeshMaterials[i] is the usemtl name of submesh i. Returns how many slots were made,
           or -1 if the file couldn't be opened. */
        int ApplyMTL(const std::string& mtlPath, const std::vector<std::string>& submeshMaterials)
        {
            std::ifstream file(mtlPath);
            if (!file)
                return -1;

            const std::filesystem::path directory = std::filesystem::path(mtlPath).parent_path();

            std::vector<MaterialSlot> parsed;
            MaterialSlot* current = nullptr;

            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream stream(line);
                std::string keyword;
                stream >> keyword;

                if (keyword == "newmtl")
                {
                    parsed.emplace_back();              // may reallocate, so take the pointer again
                    current = &parsed.back();
                    current->Albedo = glm::vec3(0.8f);
                    stream >> current->Name;
                }
                else if (!current)
                {
                    continue;                           // a line before the first newmtl
                }
                else if (keyword == "Kd")
                {
                    stream >> current->Albedo.x >> current->Albedo.y >> current->Albedo.z;
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

                    std::replace(last.begin(), last.end(), '\\', '/');   // .mtl files made on Windows
                    const std::string fullPath = (directory / last).string();
                    if (keyword == "map_Kd")      current->AlbedoMap = fullPath;
                    else if (keyword == "map_Pr") current->RoughnessMap = fullPath;
                    else if (keyword == "map_Pm") current->MetallicMap = fullPath;
                    else                          current->NormalMap = fullPath;
                }
            }

            // The albedo map carries the colour, so don't also tint it with Kd.
            for (MaterialSlot& material : parsed)
                if (!material.AlbedoMap.empty())
                    material.Albedo = glm::vec3(1.0f);

            // One slot per submesh whose material name is in the file.
            std::vector<MaterialSlot> result;
            for (size_t i = 0; i < submeshMaterials.size(); ++i)
            {
                for (const MaterialSlot& material : parsed)
                {
                    if (material.Name != submeshMaterials[i])
                        continue;

                    MaterialSlot slot = material;
                    slot.SubmeshIndex = static_cast<int>(i);
                    result.push_back(std::move(slot));
                    break;
                }
            }

            // Nothing matched by name (a one-material mesh with no usemtl): behave as before,
            // the first material on everything.
            if (result.empty() && !parsed.empty())
                result.push_back(parsed[0]);

            const int count = static_cast<int>(result.size());
            if (count > 0)
                Slots = std::move(result);
            return count;
        }
    };
}
