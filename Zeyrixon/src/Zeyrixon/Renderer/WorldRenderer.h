#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/World/World.h>

#include <glm/glm.hpp>

namespace Zeyrixon
{
   struct SpriteComponent;
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

         /* Draws every entity with a Transform and a Sprite, sorted for alpha blending. Call after
            RenderWorld (and after RenderGrid, if drawing one), with the same frame buffer bound. */
         static void RenderSprites(World& world, const glm::mat4& view, const glm::mat4& projection);

         // The sprite's quad size in world units, before the entity's scale. Shared with picking.
         static glm::vec2 GetSpriteSize(const SpriteComponent& sprite);
    };
}
