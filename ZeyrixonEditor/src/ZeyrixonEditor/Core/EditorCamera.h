#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Editor
{
    class EditorCamera
    {
    public:
        void SetViewportSize(float width, float height)
        {
            m_Aspect = height > 0.f ? width / height : 1.f;
        }

        glm::vec3 GetPosition() const
        {
            return m_Focus + m_Distance * glm::vec3(
                std::cos(m_Pitch) * std::sin(m_Yaw),
                std::sin(m_Pitch),
                std::cos(m_Pitch) * std::cos(m_Yaw)
            );
        }

        glm::mat4 GetView() const
        {
            return glm::lookAt(GetPosition(), m_Focus, glm::vec3(0.f, 1.f, 0.f));
        }

        glm::mat4 GetProjection() const
        {
            return glm::perspective(glm::radians(m_Fov), m_Aspect, m_Near, m_Far);
        }

        // Call while the viewport window is hovered.
        void OnInput()
        {
            ImGuiIO& io = ImGui::GetIO();

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                m_Yaw   -= io.MouseDelta.x * 0.01f;
                m_Pitch += io.MouseDelta.y * 0.01f;
                m_Pitch = std::clamp(m_Pitch, -1.5f, 1.5f);
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
            {
                glm::vec3 forward   = glm::normalize(m_Focus - GetPosition());
                glm::vec3 right     = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
                glm::vec3 up        = glm::cross(right, forward);
                float speed = m_Distance * 0.002f;
                m_Focus += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * speed;
            }
            
            if (io.MouseWheel != 0.f)
                m_Distance = std::max(0.5f, m_Distance * (1.f - io.MouseWheel * 0.1f));
        }
    private:
        glm::vec3 m_Focus{ 0.0f };
        float m_Distance = 8.0f;
        float m_Yaw = 0.6f;
        float m_Pitch = 0.4f;

        float m_Fov = 45.f;
        float m_Near = 0.1f;
        float m_Far = 1000.f;
        float m_Aspect = 16.f / 9.f;
    };
}