#include <Editor/Public/Common/EditorCamera.h>

EditorCamera::EditorCamera(glm::vec3 startPosition, glm::vec3 startUp, float startYaw, float startPitch)
                            : front(glm::vec3(0.0f, 0.0f, -1.0f)), 
                              movementSpeed(2.5f), 
                              mouseSensitivity(0.1f), 
                              fov(45.0f)
{
    position = startPosition;
    worldUp = startUp;
    yaw = startYaw;
    pitch = startPitch;
    UpdateCameraVectors();
}

void EditorCamera::ProcessKeyboard(CameraMovement dir, float dt)
{
    float velocity = movementSpeed * dt;
    if (dir == CameraMovement::FORWARD) position += front * velocity;
    if (dir == CameraMovement::BACKWARD) position -= front * velocity;
    if (dir == CameraMovement::LEFT) position -= right * velocity;
    if (dir == CameraMovement::RIGHT) position += right * velocity;
}

void EditorCamera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (constrainPitch) {
        if (pitch > 89.0f)  pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    UpdateCameraVectors();
}

void EditorCamera::ProcessMouseScroll(float yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)  fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

void EditorCamera::UpdateCameraVectors()
{
     glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
        
    // Re-calculate the Right and Up vector
    right = glm::normalize(glm::cross(front, worldUp));  
    up    = glm::normalize(glm::cross(right, front));
}