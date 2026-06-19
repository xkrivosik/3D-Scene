#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch, float speed, float sensitivity)
    : position(position), worldUp(up), yaw(yaw), pitch(pitch),
      movementSpeed(speed), mouseSensitivity(sensitivity)
{
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

// ==========================
// Movement
// ==========================
void Camera::moveForward(float dt) {
    position += front * (movementSpeed * dt);
}
void Camera::moveBackward(float dt) {
    position -= front * (movementSpeed * dt);
}
void Camera::moveLeft(float dt) {
    position -= glm::normalize(glm::cross(front, up)) * (movementSpeed * dt);
}
void Camera::moveRight(float dt) {
    position += glm::normalize(glm::cross(front, up)) * (movementSpeed * dt);
}

// ==========================
// Mouse movement
// ==========================
void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    // prevent gimbal lock
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    right = glm::normalize(glm::cross(front, worldUp));
    up    = glm::normalize(glm::cross(right, front));
}
