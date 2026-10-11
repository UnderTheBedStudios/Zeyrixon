#pragma once

#include <Zeyrixon/Core/Input.h>
#include <Zeyrixon/Core/KeyCodes.h>
#include <Zeyrixon/Core/MouseButtonCodes.h>
#include <Zeyrixon/Core/DeltaTime.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Editor
{
    class EditorCamera
    {
    public:
        enum class ProjectionType   { Perspective, Orthographic };
        enum class ViewType         { View3D, View2D };

        void SetProjectionType(ProjectionType type) { m_ProjectionType = type; }
        ProjectionType GetProjectionType() const { return m_ProjectionType; }

        // True when the view is orthographic, whether chosen (3D) or forced (2D).
        bool IsOrthographic() const { return Is2D() || m_ProjectionType == ProjectionType::Orthographic; }

        void SetViewType(ViewType type) { m_ViewType = type; }
        ViewType GetViewType() const { return m_ViewType; }
        bool Is3D() const { return m_ViewType == ViewType::View3D; }
        bool Is2D() const { return m_ViewType == ViewType::View2D; }

        void SetViewportSize(float width, float height)
        {
            m_ViewportHeight = height;
            m_Aspect = height > 0.f ? width / height : 1.f;
        }

        // The actual camera position for the current view type.
        glm::vec3 GetPosition() const
        {
            return Is2D() ? glm::vec3(m_Position2D, k2DCameraZ) : m_Position;
        }

        glm::vec2 GetPosition2D() const { return m_Position2D; }

        // 3D: yaw 0 / pitch 0 looks down -Z, positive pitch looks up.
        // 2D: always looks down -Z.
        glm::vec3 GetForward() const
        {
            if (Is2D())
                return glm::vec3(0.f, 0.f, -1.f);

            return glm::vec3(
                -std::sin(m_Yaw) * std::cos(m_Pitch),
                 std::sin(m_Pitch),
                -std::cos(m_Yaw) * std::cos(m_Pitch));
        }

        glm::mat4 GetView() const
        {
            const glm::vec3 eye = GetPosition();
            return glm::lookAt(eye, eye + GetForward(), glm::vec3(0.f, 1.f, 0.f));
        }

        glm::mat4 GetProjection() const
        {
            if (IsOrthographic())
            {
                // 3D: same visible height at the pivot as the perspective view, so switching
                // doesn't change the apparent zoom. 2D: its own fixed size.
                const float halfHeight = Is2D()
                    ? m_Size2D * m_2DZoom
                    : m_Distance * std::tan(glm::radians(m_Fov) * 0.5f);
                const float halfWidth = halfHeight * m_Aspect;

                // Negative near plane so geometry behind the camera position stays visible.
                return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -m_Far, m_Far);
            }

            return glm::perspective(glm::radians(m_Fov), m_Aspect, m_Near, m_Far);
        }

        // Call once per frame. A drag can only START while the viewport is hovered,
        // but it keeps going until the buttons are released.
        void OnUpdate(Zeyrixon::DeltaTime dt, bool viewportHovered)
        {
            using Zeyrixon::Input;

            const auto [mouseX, mouseY] = Input::GetMousePos();
            const glm::vec2 mouse{ mouseX, mouseY };
            const glm::vec2 delta = mouse - m_LastMouse;
            m_LastMouse = mouse;   // updated every frame so the first drag frame has no jump

            // 2D is pan-only, so look and orbit are ignored there (and never lock the cursor).
            const bool lookHeld  = Is3D() && Input::IsMouseButtonPressed(Z_MOUSE_BUTTON_RIGHT);
            const bool panHeld   = Input::IsMouseButtonPressed(Z_MOUSE_BUTTON_MIDDLE);
            const bool orbitHeld = Is3D() && Input::IsKeyPressed(Z_KEY_LEFT_ALT)
                                          && Input::IsMouseButtonPressed(Z_MOUSE_BUTTON_LEFT);

            const bool justPressed = (lookHeld  && !m_PrevLookHeld)
                                  || (panHeld   && !m_PrevPanHeld)
                                  || (orbitHeld && !m_PrevOrbitHeld);
            m_PrevLookHeld = lookHeld;
            m_PrevPanHeld = panHeld;
            m_PrevOrbitHeld = orbitHeld;

            const bool anyHeld = lookHeld || panHeld || orbitHeld;

            if (!m_Dragging && justPressed && viewportHovered)
            {
                m_Dragging = true;
                Input::SetCursorMode(Zeyrixon::CursorMode::Locked);
            }
            if (m_Dragging && !anyHeld)
            {
                m_Dragging = false;
                Input::SetCursorMode(Zeyrixon::CursorMode::Normal);
            }

            if (!m_Dragging)
                return;

            if (Is2D())
            {
                // World units per screen pixel, so the scene follows the mouse 1:1.
                if (panHeld && m_ViewportHeight > 0.f)
                {
                    const float worldPerPixel = (2.f * m_Size2D) / m_ViewportHeight;
                    m_Position2D += glm::vec2(-delta.x, delta.y) * worldPerPixel;
                }
                return;
            }

            const glm::vec3 forward = GetForward();
            const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
            const glm::vec3 up      = glm::cross(right, forward);

            if (orbitHeld)
            {
                // Orbit around a pivot m_Distance in front of the camera:
                // turn the view, then put the camera back the same distance from the pivot.
                const glm::vec3 pivot = m_Position + forward * m_Distance;
                Rotate(delta);
                m_Position = pivot - GetForward() * m_Distance;
            }
            else if (lookHeld)
            {
                Rotate(delta);

                const float speed = (Input::IsKeyPressed(Z_KEY_LEFT_SHIFT) ? m_BoostSpeed : m_MoveSpeed)
                                  * dt.GetSeconds();

                if (Input::IsKeyPressed(Z_KEY_W)) m_Position += forward * speed;
                if (Input::IsKeyPressed(Z_KEY_S)) m_Position -= forward * speed;
                if (Input::IsKeyPressed(Z_KEY_D)) m_Position += right * speed;
                if (Input::IsKeyPressed(Z_KEY_A)) m_Position -= right * speed;
                if (Input::IsKeyPressed(Z_KEY_E)) m_Position += up * speed;
                if (Input::IsKeyPressed(Z_KEY_Q)) m_Position -= up * speed;
            }
            else if (panHeld)
            {
                m_Position += (-right * delta.x + up * delta.y) * (m_Distance * 0.002f);
            }
        }

        // Scroll can't be polled, so the editor forwards MouseScrolledEvent here.
        // 3D: dollies the camera toward/away from the point m_Distance in front of it.
        void OnMouseScroll(float yOffset)
        {
            // 2D is pan-only, so scroll does nothing there.
            if (Is2D())
            {
                m_2DZoom = m_2DZoom + ((yOffset * 0.05f) * -1.f);
                m_2DZoom = std::clamp(m_2DZoom, 0.000001f, 1000.f);
            }
            else
            {
                const float newDistance = std::max(0.5f, m_Distance * (1.f - yOffset * 0.1f));
                m_Position += GetForward() * (m_Distance - newDistance);
                m_Distance = newDistance;
            }
        }

        /* A wold-space ray through a point in the viewport. 'point' is in pixels from the
           viewport's top-left corner. */
        void ScreenPointToRay(const glm::vec2& point, const glm::vec2& viewportSize,
                              glm::vec3& origin, glm::vec3& direction) const
        {
            const glm::vec2 ndc(
                (point.x / viewportSize.x) * 2.f - 1.f,
                1.f - (point.y / viewportSize.y) * 2.f);   // screen y points down, NDC y points up

            const glm::mat4 inverseViewProjection = glm::inverse(GetProjection() * GetView());

            glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc, -1.f, 1.f);
            glm::vec4 farPoint  = inverseViewProjection * glm::vec4(ndc,  1.f, 1.f);
            nearPoint /= nearPoint.w;
            farPoint  /= farPoint.w;

            origin = glm::vec3(nearPoint);
            direction = glm::normalize(glm::vec3(farPoint - nearPoint));
        }

    private:
        void Rotate(const glm::vec2& mouseDelta)
        {
            m_Yaw   -= mouseDelta.x * 0.003f;   // mouse right -> turn right
            m_Pitch -= mouseDelta.y * 0.003f;   // mouse down  -> look down
            m_Pitch  = std::clamp(m_Pitch, -1.5f, 1.5f);
        }

        ViewType m_ViewType = ViewType::View3D;
        ProjectionType m_ProjectionType = ProjectionType::Perspective;

        // 3D state. Starts where the old orbit camera did: looking at the origin from a few units away.
        glm::vec3 m_Position{ 4.16f, 3.12f, 6.08f };
        float m_Yaw = 0.6f;
        float m_Pitch = -0.4f;

        // Distance to the pivot used by 3D orbit, scroll and pan speed.
        float m_Distance = 8.f;

        // 2D state. The camera sits k2DCameraZ units along +Z looking down -Z, so a
        // 2D scene at z = 0 is comfortably inside the depth range.
        static constexpr float k2DCameraZ = 500.f;
        glm::vec2 m_Position2D{ 0.f, 0.f };
        float m_Size2D = 5.f;   // half the visible height, in world units

        float m_MoveSpeed = 5.f;
        float m_BoostSpeed = 20.f;

        float m_Fov = 45.f;
        float m_2DZoom = 1.f;
        float m_Near = 0.1f;
        float m_Far = 1000.f;
        float m_Aspect = 16.f / 9.f;
        float m_ViewportHeight = 720.f;

        glm::vec2 m_LastMouse{ 0.f };
        bool m_Dragging = false;
        bool m_PrevLookHeld = false;
        bool m_PrevPanHeld = false;
        bool m_PrevOrbitHeld = false;
    };
}
