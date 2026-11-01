#include <pch.h>

#include <Zeyrixon/Physics/Collider.h>
#include <Zeyrixon/Renderer/DebugRenderer.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Zeyrixon::Physics
{
    namespace
    {
        // A zero-sized shape confuses Bullet (and divides by zero in the editor), so nothing gets smaller than this.
        constexpr float k_MinSize = 0.001f;

        // An arc of a circle in the plane spanned by axisA and axisB, from startAngle to endAngle (radians).
        void DrawArc(const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB, float radius,
                     float startAngle, float endAngle, const glm::vec4& color, int segments)
        {
            glm::vec3 previous = center + (axisA * std::cos(startAngle) + axisB * std::sin(startAngle)) * radius;
            for (int i = 1; i <= segments; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(segments);
                const float angle = startAngle + (endAngle - startAngle) * t;
                const glm::vec3 point = center + (axisA * std::cos(angle) + axisB * std::sin(angle)) * radius;
                DebugRenderer::DrawLine(previous, point, color);
                previous = point;
            }
        }
    }

    glm::quat EulerToQuat(const glm::vec3& r)
    {
        // quaternion products compose right-to-left, like matrices: this is Rz * Ry * Rx.
        return glm::angleAxis(r.z, glm::vec3(0.f, 0.f, 1.f))
             * glm::angleAxis(r.y, glm::vec3(0.f, 1.f, 0.f))
             * glm::angleAxis(r.x, glm::vec3(1.f, 0.f, 0.f));
    }

    glm::vec3 QuatToEuler(const glm::quat& q)
    {
        /* For R = Rz(z) * Ry(y) * Rx(x), multiplying it out gives (row, column):
               R(2,0) = -sin y     R(2,1) = cos y sin x     R(2,2) = cos y cos x
               R(1,0) =  cos y sin z                        R(0,0) = cos y cos z
           which we solve for x, y, z. glm matrices are m[column][row], so R(r,c) is m[c][r]. */
        const glm::mat3 m = glm::mat3_cast(glm::normalize(q));
        const float sinY = -m[0][2];

        // cos y, recovered from two independent entries of the matrix. When it is (nearly) zero we are at
        // gimbal lock. Using atan2 for y instead of asin matters: asin is hopelessly imprecise near +-1.
        const float cosY = std::sqrt(m[1][2] * m[1][2] + m[2][2] * m[2][2]);

        float x, y, z;
        if (cosY > 1e-4f)
        {
            y = std::atan2(sinY, cosY);
            x = std::atan2(m[1][2], m[2][2]);
            z = std::atan2(m[0][1], m[0][0]);
        }
        else
        {
            // Gimbal lock (looking straight up/down): x and z describe the same rotation, so pin z to 0.
            y = sinY > 0.0f ? glm::half_pi<float>() : -glm::half_pi<float>();
            z = 0.0f;
            x = sinY > 0.0f ? std::atan2(m[1][0], m[1][1]) : std::atan2(-m[1][0], m[1][1]);
        }
        return glm::vec3(x, y, z);
    }

    ColliderShapeWorld ComputeWorldShape(const ColliderComponent& c, const TransformComponent& t)
    {
        ColliderShapeWorld s;
        s.Rotation = EulerToQuat(t.Rotation);

        // The offset is in the entity's local space, so it is scaled and rotated with the entity.
        s.Center = t.Position + s.Rotation * (c.Offset * t.Scale);

        // Negative scale just mirrors the shape; the size is what matters. Spheres and capsules can't be
        // squashed by a non-uniform scale (a sphere stays a sphere), so they follow the biggest relevant axis.
        const glm::vec3 scale = glm::max(glm::abs(t.Scale), glm::vec3(k_MinSize));

        s.HalfExtents = glm::max(glm::abs(c.HalfExtents) * scale, glm::vec3(k_MinSize));

        switch (c.Shape)
        {
            case ColliderShape::Sphere:
                s.Radius = std::max(c.Radius * std::max({ scale.x, scale.y, scale.z }), k_MinSize);
                s.HalfHeight = s.Radius;
                break;

            case ColliderShape::Capsule:
                s.Radius = std::max(c.Radius * std::max(scale.x, scale.z), k_MinSize);
                s.HalfHeight = std::max(c.Height * 0.5f * scale.y, s.Radius);   // never shorter than a sphere
                break;

            default:
                s.Radius = k_MinSize;
                s.HalfHeight = k_MinSize;
                break;
        }

        return s;
    }

    void DrawColliderWireframe(const ColliderComponent& collider, const TransformComponent& transform,
                               const glm::vec4& color)
    {
        const ColliderShapeWorld s = ComputeWorldShape(collider, transform);

        // The collider's local axes, as they point in the world.
        const glm::vec3 axisX = s.Rotation * glm::vec3(1.f, 0.f, 0.f);
        const glm::vec3 axisY = s.Rotation * glm::vec3(0.f, 1.f, 0.f);
        const glm::vec3 axisZ = s.Rotation * glm::vec3(0.f, 0.f, 1.f);

        switch (collider.Shape)
        {
            case ColliderShape::Box:
            {
                // 8 corners, indexed by three bits: bit 0 = +X, bit 1 = +Y, bit 2 = +Z.
                glm::vec3 corners[8];
                for (int i = 0; i < 8; ++i)
                {
                    corners[i] = s.Center
                        + axisX * ((i & 1) ? s.HalfExtents.x : -s.HalfExtents.x)
                        + axisY * ((i & 2) ? s.HalfExtents.y : -s.HalfExtents.y)
                        + axisZ * ((i & 4) ? s.HalfExtents.z : -s.HalfExtents.z);
                }

                // An edge joins two corners that differ in exactly one bit.
                for (int i = 0; i < 8; ++i)
                    for (int bit = 1; bit <= 4; bit <<= 1)
                        if (!(i & bit))
                            DebugRenderer::DrawLine(corners[i], corners[i | bit], color);
                break;
            }

            case ColliderShape::Sphere:
            {
                // Three great circles, one per axis plane.
                DebugRenderer::DrawCircle(s.Center, axisX, axisY, s.Radius, color, 48);
                DebugRenderer::DrawCircle(s.Center, axisY, axisZ, s.Radius, color, 48);
                DebugRenderer::DrawCircle(s.Center, axisZ, axisX, s.Radius, color, 48);
                break;
            }

            case ColliderShape::Capsule:
            {
                // The straight part runs between two sphere centres; the caps are hemispheres around them.
                const float cylinderHalf = std::max(s.HalfHeight - s.Radius, 0.0f);
                const glm::vec3 top = s.Center + axisY * cylinderHalf;
                const glm::vec3 bottom = s.Center - axisY * cylinderHalf;
                constexpr float pi = glm::pi<float>();

                // The two rings where the cylinder meets the caps, and four lines joining them.
                DebugRenderer::DrawCircle(top, axisX, axisZ, s.Radius, color, 32);
                DebugRenderer::DrawCircle(bottom, axisX, axisZ, s.Radius, color, 32);
                for (const glm::vec3& side : { axisX * s.Radius, -axisX * s.Radius, axisZ * s.Radius, -axisZ * s.Radius })
                    DebugRenderer::DrawLine(top + side, bottom + side, color);

                // Half circles for the caps, in the XY and ZY planes (angle 0..pi goes up over the top).
                DrawArc(top, axisX, axisY, s.Radius, 0.0f, pi, color, 16);
                DrawArc(top, axisZ, axisY, s.Radius, 0.0f, pi, color, 16);
                DrawArc(bottom, axisX, axisY, s.Radius, pi, 2.0f * pi, color, 16);
                DrawArc(bottom, axisZ, axisY, s.Radius, pi, 2.0f * pi, color, 16);
                break;
            }
        }
    }
}
