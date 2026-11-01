#pragma once

#include <Zeyrixon/Core/Core.h>

#include <glm/glm.hpp>

namespace Zeyrixon
{
    /* Immediate-mode debug lines: queue lines with DrawLine / DrawCircle during a frame, then draw them all
       with ONE draw call using Flush while the target framebuffer is bound. Used for the physics wireframes,
       but it knows nothing about physics, so it is reusable for ray visualisation, AI paths, bounds, ...

       Flush always empties the queue, so a frame that queues lines but never flushes would carry them into
       the next one; call Flush every frame you queue. */
    class Z_API DebugRenderer
    {
    public:
        static void DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);

        // A circle in the plane spanned by axisA and axisB (both unit length, perpendicular).
        static void DrawCircle(const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB,
                               float radius, const glm::vec4& color, int segments = 32);

        /* Draws and clears the queue. With seeThrough, the parts of lines hidden behind geometry are still
           drawn, faintly, so a collider buried inside a mesh doesn't simply vanish. */
        static void Flush(const glm::mat4& view, const glm::mat4& projection, bool seeThrough = true);
    };
}
