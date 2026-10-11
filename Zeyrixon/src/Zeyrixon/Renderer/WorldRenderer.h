#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/World.h>

#include <glm/glm.hpp>

namespace Zeyrixon
{
    class Z_API WorldRenderer
    {
    public:
        /* Draws every entity that has a Transform and a Mesh into the framebuffer
           that is currently bound. The caller binds, clears and unbinds. */
        static void RenderWorld(World& world, const glm::mat4& view, const glm::mat4& projection,
                                const glm::vec3& cameraPosition);

        /* Draws a depth-tested reference grid. Call after RenderWorld, with the same framebuffer
           bound. xyPlane = false: the grid lies on the ground (XZ), for 3D. true: on the XY plane,
           for 2D view. */
        static void RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                               const glm::vec3& cameraPosition, bool xyPlane);
    };
}
