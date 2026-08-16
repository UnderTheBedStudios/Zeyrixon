#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

enum class MouseButton {
    Left,
    Right,
    Middle
};

class EditorCamera {
public:
    // Camera Vectors
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler Angles
    float yaw;
    float pitch;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float panSensitivity;
    float fov;

    EditorCamera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f),
           float startYaw = -90.0f,
           float startPitch = 0.0f);

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void ProcessKeyboard(CameraMovement dir, float deltaTime);

    // dt before the defaulted bool — fixes the illegal default-arg ordering
    void ProcessMouseMovement(float xoffset, float yoffset, float dt, bool constrainPitch = true);

    void ProcessMouseButtonPressed(MouseButton button);
    void ProcessMouseButtonReleased(MouseButton button);

    void ProcessMouseScroll(float yoffset);

    bool IsFlying()   const { return m_Flying; }
    bool IsPivoting() const { return m_Pivoting; }

private:
    void UpdateCameraVectors();

    bool m_Flying;
    bool m_Pivoting;
};