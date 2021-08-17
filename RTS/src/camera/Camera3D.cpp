#include "stdafx.h"
#include "Camera3D.h"

#include <SDL2/SDL.h>
#include <Vorb/utils.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define UP_ABSOLUTE (f32v3(0.0f, 1.0f, 0.0f))

Camera3D::Camera3D() {
    // Empty
}

void Camera3D::init(float aspectRatio) {
    mAspectRatio = aspectRatio;
}

void Camera3D::offsetPosition(const f32v3& offset) {
    mPosition += offset;
    mViewChanged = true;
}

void Camera3D::update() {

    bool updateFrustum = false;
    if (mViewChanged) {
        updateView();
        mViewChanged = false;
        updateFrustum = true;
    }
    if (mProjectionChanged) {
        updateProjection();
        mProjectionChanged = false;
        updateFrustum = true;
    }

    if (updateFrustum) {
        mVP = mP * mV;
        mFrustum.updateFromWVP(mVP);
    }
}

void Camera3D::updateView() {
    mV = glm::lookAt(f32v3(mPosition), f32v3(mPosition) + mDirection, mUp);
}

void Camera3D::updateProjection() {
    mFrustum.setCamInternals(mFieldOfView, mAspectRatio, mZNear, mZFar);
    mP = glm::perspective(glm::radians(mFieldOfView), mAspectRatio, mZNear, mZFar);
}

void Camera3D::applyRotation(const f32q& rot) {
    mDirection = rot * mDirection;
    mRight = rot * mRight;
    mUp = glm::normalize(glm::cross(mRight, mDirection));

    mViewChanged = true;
}

void Camera3D::rotateFromMouseAbsoluteUp(float dx, float dy, float speed, bool clampVerticalRotation /* = false*/) {
    f32q upQuat = glm::angleAxis(dy * speed, mRight);
    f32q rightQuat = glm::angleAxis(dx * speed, UP_ABSOLUTE);

    f32v3 previousDirection = mDirection;
    f32v3 previousUp = mUp;
    f32v3 previousRight = mRight;

    applyRotation(upQuat * rightQuat);

    if (clampVerticalRotation && mUp.y < 0) {
        mDirection = previousDirection;
        mUp = previousUp;
        mRight = previousRight;
        rotateFromMouseAbsoluteUp(dx, 0.0f, speed);
    }
}

void Camera3D::rotateFromMouse(float dx, float dy, float speed) {
    f32q upQuat = glm::angleAxis(dy * speed, mRight);
    f32q rightQuat = glm::angleAxis(dx * speed, mUp);

    applyRotation(upQuat * rightQuat);
}

void Camera3D::rollFromMouse(float dx, float speed) {
    f32q frontQuat = glm::angleAxis(dx * speed, mDirection);

    applyRotation(frontQuat);
}

void Camera3D::lookAt(const f32v3& pos) {
    mDirection = glm::normalize(pos - mPosition);
    mRight = glm::normalize(glm::cross(UP_ABSOLUTE, mDirection));
    mUp = glm::normalize(glm::cross(mDirection, mRight));
    mViewChanged = true;
}

void Camera3D::setOrientation(const f32q& orientation) {
    mDirection = orientation * f32v3(0.0, 0.0, 1.0);
    mRight = orientation * f32v3(1.0, 0.0, 0.0);
    mUp = orientation * f32v3(0.0, 1.0, 0.0);
    mViewChanged = true;
}

f32v3 Camera3D::worldToScreenPoint(const f32v3& worldPoint) const {
    // Transform world to clipping coordinates
    f32v4 clipPoint = mVP * f32v4(worldPoint, 1.0f);
    clipPoint.x /= clipPoint.w;
    clipPoint.y /= clipPoint.w;
    clipPoint.z /= clipPoint.w;
    return f32v3((clipPoint.x + 1.0) / 2.0f,
        (1.0 - clipPoint.y) / 2.0f,
        clipPoint.z);
}

f32v3 Camera3D::worldToScreenPointLogZ(const f32v3& worldPoint, f32 zFar) const {
    // Transform world to clipping coordinates
    f32v4 clipPoint = mVP * f32v4(worldPoint, 1.0f);
    clipPoint.z = log2(glm::max(0.0001f, clipPoint.w + 1.0f)) * 2.0f / log2(zFar + 1.0f) - 1.0f;
    clipPoint.x /= clipPoint.w;
    clipPoint.y /= clipPoint.w;
    return f32v3((clipPoint.x + 1.0f) / 2.0f,
        (1.0f - clipPoint.y) / 2.0f,
        clipPoint.z);
}

f32v3 Camera3D::getPickRay(const f32v2& ndcScreenPos) const {
    f32v4 clipRay(ndcScreenPos.x, ndcScreenPos.y, -1.0f, 1.0f);
    f32v4 eyeRay = glm::inverse(mP) * clipRay;
    eyeRay = f32v4(eyeRay.x, eyeRay.y, -1.0f, 0.0f);
    return glm::normalize(f32v3(glm::inverse(mV) * eyeRay));
}
