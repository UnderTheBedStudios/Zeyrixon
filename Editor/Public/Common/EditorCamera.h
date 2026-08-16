#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

enum CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
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
    float fov;

    EditorCamera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f), 
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f), 
           float startYaw = -90.0f, 
           float startPitch = 0.0f);

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    // Processes input received from any keyboard-like input system
    void ProcessKeyboard(CameraMovement dir, float deltaTime);

    // Processes input received from a mouse input system
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    // Processes input received from a mouse scroll wheel event
    void ProcessMouseScroll(float yoffset);

private:
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void UpdateCameraVectors();
};