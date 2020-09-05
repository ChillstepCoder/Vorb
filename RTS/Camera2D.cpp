#include "stdafx.h"
#include "Camera2D.h"

 Camera2D::Camera2D() : _position(0.0f, 0.0f),
    _cameraMatrix(1.0f),
    _orthoMatrix(1.0f),
    _scale(1.0f),
    _needsMatrixUpdate(true),
    _screenWidth(500),
    _screenHeight(500)
{
}

Camera2D::~Camera2D()
{
}

void Camera2D::init(int screenWidth, int screenHeight) {
    _screenWidth = screenWidth;
    _screenHeight = screenHeight;
    _orthoMatrix = glm::ortho(0.0f, (float)_screenWidth, 0.0f, (float)_screenHeight);
}

// TODO: Investigate Pixels per unit
// https://blogs.unity3d.com/2015/06/19/pixel-perfect-2d/
// const int PPU = 16;

// updates the camera matrix if needed
void Camera2D::update() {

    // Only update if our position or scale have changed
	if (_needsMatrixUpdate) {

		// Round for fixing grid issues
        const float roundScale = round(_scale);

        // Camera Translation
        glm::vec3 translate(round(-_position.x * roundScale + _screenWidth / 2), round(-_position.y * roundScale + _screenHeight / 2), 0.0f);
        _cameraMatrix = glm::translate(_orthoMatrix, translate);

        // Camera Scale
        glm::vec3 scale(roundScale, roundScale, 1.0f);
        _cameraMatrix = glm::scale(_cameraMatrix, scale);

        _needsMatrixUpdate = false;
    }
}

glm::vec2 Camera2D::convertScreenToWorld(const glm::vec2& screenCoords) const {
    const f32m4 invCamera = glm::inverse(_cameraMatrix);
    const f32v2 scaledCoords = (f32v2(screenCoords.x / _screenWidth, 1.0f - screenCoords.y / _screenHeight) - 0.5f) * 2.0f;
    return f32v2(f32v4(scaledCoords, 0.0f, 0.0f) * invCamera) + _position;
}

// Simple AABB test to see if a box is in the camera view
bool Camera2D::isBoxInView(const glm::vec2& position, const glm::vec2& dimensions) {

    glm::vec2 scaledScreenDimensions = glm::vec2(_screenWidth, _screenHeight) / (_scale);

    // The minimum distance before a collision occurs
    const float MIN_DISTANCE_X = dimensions.x / 2.0f + scaledScreenDimensions.x / 2.0f;
    const float MIN_DISTANCE_Y = dimensions.y / 2.0f + scaledScreenDimensions.y / 2.0f;

    // Center position of the parameters
    glm::vec2 centerPos = position + dimensions / 2.0f;
    // Center position of the camera
    glm::vec2 centerCameraPos = _position * _scale;
    // Vector from the input to the camera
    glm::vec2 distVec = centerPos - centerCameraPos;

    // Get the depth of the collision
    float xDepth = MIN_DISTANCE_X - abs(distVec.x);
    float yDepth = MIN_DISTANCE_Y - abs(distVec.y);

    // If both the depths are > 0, then we collided
    if (xDepth > 0 && yDepth > 0) {
        // There was a collision
        return true;
    }
    return false;
}