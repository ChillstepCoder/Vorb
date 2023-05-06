#include "stdafx.h"
#include "Camera3D.h"

#include <SDL2/SDL.h>
#include <Vorb/utils.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util/Utils.h"

#include "options/DebugOptions.h"

#define UP_ABSOLUTE (f32v3(0.0f, 0.0f, 1.0f))

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

// TODO: Update should take delta time and interpolate it
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
        mInverseVP = glm::inverse(mVP);
        if (!sDebugOptions.mPauseFrustum) {
            mFrustum.updateFromWVP(mVP);
        }
    }
}

void Camera3D::updateView() {
    mV = glm::lookAt(f32v3(0.0f), mDirection, mUp);
    mInverseV = glm::inverse(mV);
}

void Camera3D::updateProjection() {
    if (!sDebugOptions.mPauseFrustum) {
        mFrustum.setCamInternals(mFieldOfView, mAspectRatio, mZNear, mZFar);
    }
    mP = glm::perspective(glm::radians(mFieldOfView), mAspectRatio, mZNear, mZFar);
    mInverseP = glm::inverse(mP);
}

void Camera3D::applyRotation(const f32q& rot) {
    mDirection = rot * mDirection;
    mRight = rot * mRight;
    mRight = glm::normalize(mRight);

    mUp = glm::normalize(glm::cross(mRight, mDirection));

    mViewChanged = true;
}

void Camera3D::applyRotation(const f32 pitch, const f32 yaw) {
    mPitch += pitch;
    f32 oldYaw = mYaw.load();
    mYaw = oldYaw + yaw;

    mPitch = glm::clamp(mPitch, -M_PI_4F, M_PI_4F);

    mDirection.x = sin(mYaw) * cos(mPitch);
    mDirection.y = cos(mYaw) * cos(mPitch);
    mDirection.z = -sin(mPitch);

    mRight.x = cos(mYaw);
    mRight.y = -sin(mYaw);
    mRight.z = 0.0;

    mDirection = glm::normalize(mDirection);
    mRight = glm::normalize(mRight);
    mUp = glm::cross(mRight, mDirection);

    mViewChanged = true;
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
    mRight = glm::normalize(glm::cross(mDirection, UP_ABSOLUTE));
    mUp = glm::normalize(glm::cross(mRight, mDirection));
    assert(abs(mRight.z) <= MATH_EPSILON);

    mPitch = asin(-mDirection.z);
    mYaw = atan2(mDirection.x, mDirection.y);

    mViewChanged = true;
}

void Camera3D::setOrientation(const f32q& orientation) {
    mDirection = orientation * f32v3(0.0, 0.0, 1.0);
    mRight = orientation * f32v3(1.0, 0.0, 0.0);
    mUp = orientation * f32v3(0.0, 1.0, 0.0);

    mPitch = asin(-mDirection.z);
    mYaw = atan2(mDirection.x, mDirection.y);

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
    f32v4 eyeRay = mInverseP * clipRay;
    eyeRay = f32v4(eyeRay.x, eyeRay.y, -1.0f, 0.0f);
    return glm::normalize(f32v3(mInverseV * eyeRay));
}
