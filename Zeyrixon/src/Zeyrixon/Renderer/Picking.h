#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>

#include <glm/glm.hpp>

namespace Zeyrixon
{
    class Z_API Picking
    {
    public:
        /* Returns the nearest entity with a Mesh that the world-space ray hits, or an invalid
           Entity (test it with 'if (entity)') if the ray hits nothing. */
        static Entity PickEntity(World& world, const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
    };
}
