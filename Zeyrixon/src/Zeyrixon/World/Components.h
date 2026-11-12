#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace Zeyrixon
{
    /* The entity's ID, plus where it sits in the parent/child hierarchy. Never edit ParentID or ChildIDs
       by hand: use World::SetParent, which keeps both sides in step (and refuses loops). Only ParentID is
       saved; ChildIDs is rebuilt from it when a world loads. */
    struct IDComponent
    {
        uint64_t ID = 0;
        uint64_t ParentID = 0;            // 0 = no parent (this entity is a root)
        std::vector<uint64_t> ChildIDs;   // in order; the outliner shows them in this order

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

        /* The reverse of GetMatrix: takes a matrix apart into Position / Rotation / Scale, using the same
           T * Rz * Ry * Rx * S order. Used to turn a world-space matrix back into a local transform
           (reparenting, gizmo drags on children). A matrix that is sheared (a rotated child under a
           non-uniformly scaled parent) can't be stored exactly in this form; the closest fit is kept. */
        void SetFromMatrix(const glm::mat4& m)
        {
            Position = glm::vec3(m[3]);

            glm::vec3 axisX(m[0]), axisY(m[1]), axisZ(m[2]);
            Scale = glm::vec3(glm::length(axisX), glm::length(axisY), glm::length(axisZ));

            // A mirrored matrix has a negative determinant; put the minus sign on X so the rotation stays proper.
            if (glm::dot(glm::cross(axisX, axisY), axisZ) < 0.0f)
                Scale.x = -Scale.x;

            const glm::vec3 safe(Scale.x != 0.0f ? Scale.x : 1.0f, Scale.y != 0.0f ? Scale.y : 1.0f, Scale.z != 0.0f ? Scale.z : 1.0f);
            axisX /= safe.x; axisY /= safe.y; axisZ /= safe.z;

            // With R = Rz * Ry * Rx:  R(2,0) = -sin(y),  R(2,1) = sin(x)cos(y),  R(2,2) = cos(x)cos(y),
            // R(1,0) = cos(y)sin(z),  R(0,0) = cos(y)cos(z).  (axisN is column N, so R(row, N) = axisN[row].)
            const float sinY = glm::clamp(-axisX.z, -1.0f, 1.0f);
            Rotation.y = std::asin(sinY);
            if (std::abs(sinY) < 0.99999f)
            {
                Rotation.x = std::atan2(axisY.z, axisZ.z);
                Rotation.z = std::atan2(axisX.y, axisX.x);
            }
            else
            {
                // Looking straight up/down (gimbal lock): x and z do the same thing, so put it all in x.
                Rotation.x = std::atan2(-axisZ.y, axisY.y);
                Rotation.z = 0.0f;
            }
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

    enum class LightType : int
    {
        Directional = 0,   // the sun: parallel rays, no position, no falloff
        Point       = 1,   // a bulb: shines in every direction from its position
        Spot        = 2    // a torch: a point light squeezed into a cone
    };

    /* A light source. Where it is and which way it faces come from the entity's Transform:
       the position is Transform.Position, and the light shines down its own local -Z axis
       (the same convention cameras use). Scale is ignored. */
    struct LightComponent
    {
        LightType Type = LightType::Point;
        glm::vec3 Color{ 1.0f };
        float Intensity = 10.0f;        // multiplies Color. Directional lights want ~3, point lights ~10, spots ~30
        float Range = 15.0f;            // world units at which a point/spot light has faded to nothing
        float InnerConeAngle = 20.0f;   // degrees; spot only: full brightness inside this angle...
        float OuterConeAngle = 30.0f;   // degrees; spot only: ...fading to nothing at this one

        /* Shadows. For now only directional lights cast them, and only ONE light does: the first
           directional light in the world that has CastShadows on. */
        bool CastShadows = true;
        float ShadowDistance = 50.0f;   // radius (world units) around the camera that gets shadows. Smaller = sharper
        float ShadowBias = 0.05f;       // world units; raise it if surfaces are speckled with shadow ("acne")
        float ShadowNormalBias = 1.5f;  // in shadow-map texels; raise it if acne remains on slanted surfaces

        // The direction this light travels (local -Z, rotated by the transform's rotation).
        static glm::vec3 GetDirection(const TransformComponent& transform)
        {
            const glm::mat4 identity(1.f);
            const glm::mat4 rotation = glm::rotate(identity, transform.Rotation.z, glm::vec3(0.f, 0.f, 1.f))
                * glm::rotate(identity, transform.Rotation.y, glm::vec3(0.f, 1.f, 0.f))
                * glm::rotate(identity, transform.Rotation.x, glm::vec3(1.f, 0.f, 0.f));
            return glm::normalize(glm::vec3(rotation * glm::vec4(0.f, 0.f, -1.f, 0.f)));
        }

        // The same, from a WORLD matrix (so a light parented to something still points the right way).
        static glm::vec3 GetDirection(const glm::mat4& world)
        {
            const glm::vec3 backwards(world[2]);   // local +Z in world space; scale along Z doesn't change its direction
            return glm::length(backwards) > 1e-6f ? -glm::normalize(backwards) : glm::vec3(0.f, 0.f, -1.f);
        }
    };

    struct MeshComponent
    {
        // "builtin:cube", or the path of an .obj file.
        std::string MeshPath = "builtin:cube";
    };

    enum class ColliderShape : int
    {
        Box     = 0,
        Sphere  = 1,
        Capsule = 2    // upright along the entity's local Y axis, like a pill
    };

    /* One collision shape. An entity's collider is a LIST of these (see ColliderComponent), the same way a
       mesh's materials are a list of MaterialSlots: a character can be a capsule for the body plus a sphere
       for the head, a table a box for the top plus four thin boxes for the legs. Everything is in the
       entity's local space, so the entity's Transform (including Scale) is applied on top: a half-extent of
       0.5 on an entity with scale 2 is a box 2 units wide. See Physics/Collider.h for how a slot becomes a
       world-space shape. */
    struct ColliderSlot
    {
        std::string Name;                // free text for you ("Head", "Left leg"); shown in the inspector
        int SubmeshIndex = -1;           // editor only: which submesh "Fit to Mesh" wraps. -1 = the whole mesh.
                                         // Physics never reads it; a shape stays where you put it.

        ColliderShape Shape = ColliderShape::Box;
        glm::vec3 Offset{ 0.0f };        // centre of this shape relative to the entity's origin
        glm::vec3 HalfExtents{ 0.5f };   // Box: half the size along X, Y, Z. (0.5 matches builtin:cube)
        float Radius = 0.5f;             // Sphere and Capsule
        float Height = 2.0f;             // Capsule: tip to tip, including both caps. Never less than 2 * Radius
    };

    /* The SHAPE of a physical object. On its own it is a static obstacle (a floor, a wall); add a RigidBody
       to make it move. EnTT allows only one component of each type per entity, so the several shapes live in
       a list inside the one component, exactly like MaterialComponent. With several slots the shapes move as
       one rigid body. */
    struct ColliderComponent
    {
        std::vector<ColliderSlot> Slots{ ColliderSlot{} };   // a new component starts with one box
    };

    enum class BodyType : int
    {
        Static    = 0,   // never moves (same as having no RigidBody at all)
        Dynamic   = 1,   // moved by the simulation: gravity, collisions, forces
        Kinematic = 2    // moved by YOU (scripts / animation), pushes dynamic bodies out of the way
    };

    /* How a Collider behaves. A RigidBody without a Collider does nothing (it has no shape to collide
       with); the engine logs a warning when Play starts. */
    struct RigidBodyComponent
    {
        BodyType Type = BodyType::Dynamic;
        float Mass = 1.0f;              // Dynamic only. Heavier bodies are harder to push, they do NOT fall faster
        float Friction = 0.5f;          // 0 = ice, 1 = rubber-on-concrete. Bullet multiplies the two surfaces' values
        float Restitution = 0.0f;       // bounciness: 0 = dead stop, 1 = loses no energy
        float LinearDamping = 0.0f;     // air resistance on movement (0..1 per second-ish)
        float AngularDamping = 0.05f;   // air resistance on spinning; a little keeps things from spinning forever
        bool UseGravity = true;
        bool FixedRotation = false;     // can't tip over; handy for characters
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

        // Values edited in the editor for fields marked [Editor] in the script.
        std::unordered_map<std::string, std::string> ExposedFields;

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
