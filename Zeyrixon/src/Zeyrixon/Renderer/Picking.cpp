#include <pch.h>

#include <Zeyrixon/Renderer/Picking.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/World/Components.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Zeyrixon
{
    namespace
    {
        // Slab test. Cheap early-out before the per-triangle work.
        bool RayIntersectsBox(const glm::vec3& origin, const glm::vec3& direction,
                              const glm::vec3& boxMin, const glm::vec3& boxMax)
        {
            float tMin = 0.f;
            float tMax = std::numeric_limits<float>::max();

            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::abs(direction[axis]) < 1e-8f)
                {
                    // Parallel to this pair of faces: it must already be between them.
                    if (origin[axis] < boxMin[axis] || origin[axis] > boxMax[axis])
                        return false;
                }
                else
                {
                    const float inverse = 1.f / direction[axis];
                    float t1 = (boxMin[axis] - origin[axis]) * inverse;
                    float t2 = (boxMax[axis] - origin[axis]) * inverse;
                    if (t1 > t2)
                        std::swap(t1, t2);

                    tMin = std::max(tMin, t1);
                    tMax = std::min(tMax, t2);
                    if (tMin > tMax)
                        return false;
                }
            }
            return true;
        }

        // Moller-Trumbore. Double-sided, because meshes are drawn without face culling.
        bool RayIntersectsTriangle(const glm::vec3& origin, const glm::vec3& direction,
                                   const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t)
        {
            const glm::vec3 edge1 = v1 - v0;
            const glm::vec3 edge2 = v2 - v0;

            const glm::vec3 p = glm::cross(direction, edge2);
            const float determinant = glm::dot(edge1, p);
            if (std::abs(determinant) < 1e-12f)
                return false; // parallel to triangle, or degenerate

            const float inverse = 1.f / determinant;
            const glm::vec3 s = origin - v0;

            const float u = glm::dot(s, p) * inverse;
            if (u < 0.f || u > 1.f)
                return false;

            const glm::vec3 q = glm::cross(s, edge1);
            const float v = glm::dot(direction, q) * inverse;
            if (v < 0.f || u + v > 1.f)
                return false;

            t = glm::dot(edge2, q) * inverse;
            return t > 0.f;
        }
    }

    Entity Picking::PickEntity(World& world, const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
    {
        auto& registry = world.GetRegistry();

        entt::entity closestEntity = entt::null;
        float closestDistance = std::numeric_limits<float>::max();

        auto entities = registry.view<TransformComponent, MeshComponent>();
        for (auto [handle, transform, meshComponent] : entities.each())
        {
            std::shared_ptr<Mesh> mesh = MeshLibrary::Get(meshComponent.MeshPath);
            if (!mesh)
                continue;

            // Test in the mesh's own space, so rotation and scale don't matter. Leaving the
            // direction unnormalised keeps the distance t the same in both spaces, so hits on
            // different entities can be compared directly.
            const glm::mat4 inverseModel = glm::inverse(transform.GetMatrix());
            const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(rayOrigin, 1.0f));
            const glm::vec3 localDirection = glm::vec3(inverseModel * glm::vec4(rayDirection, 0.0f));

            if (!RayIntersectsBox(localOrigin, localDirection, mesh->GetBoundsMin(), mesh->GetBoundsMax()))
                continue;

            const auto& vertices = mesh->GetVerticies();
            const auto& indices = mesh->GetIndices();
            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                float t = 0.0f;
                if (RayIntersectsTriangle(localOrigin, localDirection,
                                          vertices[indices[i]].Position,
                                          vertices[indices[i + 1]].Position,
                                          vertices[indices[i + 2]].Position, t)
                    && t < closestDistance)
                {
                    closestDistance = t;
                    closestEntity = handle;
                }
            }
        }

        if (closestEntity == entt::null)
            return Entity{};

        return Entity{ closestEntity, &world };
    }
}
