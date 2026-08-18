#include <Editor/Public/Common/EditorCamera.h>

EditorCamera::EditorCamera(glm::vec3 startPosition, glm::vec3 startUp, float startYaw, float startPitch)
                            : front(glm::vec3(0.0f, 0.0f, -1.0f)),
                              movementSpeed(5.f),
                              fastMovementSpeed(10.f),
                              slowMovementSpeed(2.5f),
                              mouseSensitivity(0.1f),
                              panSensitivity(0.005f),
                              fov(45.0f)
{
    position = startPosition;
    worldUp = startUp;
    yaw = startYaw;
    pitch = startPitch;
    UpdateCameraVectors();

    m_Flying = false;
    m_Pivoting = false;

    m_MoveFast = false;
    m_MoveSlow = false;
}

void EditorCamera::ProcessKeyboard(CameraMovement dir, float dt)
{
    float velocity = m_MoveFast ? fastMovementSpeed * dt :
                     m_MoveSlow ? slowMovementSpeed * dt :
                     movementSpeed * dt;
    if (dir == CameraMovement::FORWARD)  position += front * velocity;
    if (dir == CameraMovement::BACKWARD) position -= front * velocity;
    if (dir == CameraMovement::LEFT)     position -= right * velocity;
    if (dir == CameraMovement::RIGHT)    position += right * velocity;
    if (dir == CameraMovement::UP)       position += up    * velocity;
    if (dir == CameraMovement::DOWN)     position -= up    * velocity;

    if (dir == CameraMovement::FAST)       m_MoveFast = true;
    if (dir == CameraMovement::SLOW)       m_MoveSlow = true;
    if (dir == CameraMovement::NORMAL)
    {
        m_MoveFast = false;
        m_MoveSlow = false;
    }
}

void EditorCamera::ProcessMouseMovement(float xoffset, float yoffset, float dt, bool constrainPitch)
{
    if (m_Flying)
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
    else if (m_Pivoting)
    {
        // Pan along the camera's own right/up, not world axes, so it stays
        // correct once the camera has rotated. Sign: dragging the mouse
        // right pushes the camera left, so the scene appears to follow
        // the drag (standard "grab and pan" feel). Flip if you want the
        // opposite convention.
        position += right * xoffset * panSensitivity;
        position += up    * yoffset * panSensitivity;
    }
}

void EditorCamera::ProcessMouseButtonPressed(MouseButton button)
{
    if (button == MouseButton::Right)
    {
        m_Flying = true;
        m_Pivoting = false;
    }
    else if (button == MouseButton::Middle)
    {
        m_Flying = false;
        m_Pivoting = true;
    }
}

void EditorCamera::ProcessMouseButtonReleased(MouseButton button)
{
    if (button == MouseButton::Right)   m_Flying = false;
    if (button == MouseButton::Middle)  m_Pivoting = false;
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

    right = glm::normalize(glm::cross(front, worldUp));
    up    = glm::normalize(glm::cross(right, front));
}