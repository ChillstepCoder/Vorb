#include "stdafx.h"
#include "Camera3D.h"

#include <SDL2/SDL.h>
#include <Vorb/utils.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "util/Utils.h"

#include "options/DebugOptions.h"

#include "camera/SimpleCamera.h"

#define UP_ABSOLUTE (f32v3(0.0f, 0.0f, 1.0f))

Camera3D::Camera3D() {
    // Empty
}

// TODO: Update should take delta time and interpolate it
void Camera3D::postUpdate(bool changed) {
    if (changed) {
        if (!sDebugOptions.mPauseFrustum) {
            mFrustum.updateFromWVP(mMatrices.VP);
        }
    }
}


void Camera3D::updateProjection() {
    if (!sDebugOptions.mPauseFrustum) {
        mFrustum.setCamInternals(mFieldOfView, mAspectRatio, mZNear, mZFar);
    }
    mMatrices.P = glm::perspective(glm::radians(mFieldOfView), mAspectRatio, mZNear, mZFar);
    mMatrices.inverseP = glm::inverse(mMatrices.P);
}

void Camera3D::applyRotation(const f32q& rot) {
    mDirection = rot * mDirection;
    mRight = rot * mRight;
    mRight = glm::normalize(mRight);

    mUp = glm::normalize(glm::cross(mRight, mDirection));

    mDirtyView = true;
}

void Camera3D::applyRotation(const f32 pitch, const f32 yaw) {
    mPitch += pitch;
    f32 oldYaw = mYaw.load();
    mYaw = oldYaw + yaw;

    mPitch = glm::clamp(mPitch, -M_PI_2F + 0.01f, M_PI_2F - 0.01f);

    mDirection.x = cos(mYaw) * cos(mPitch);
    mDirection.y = sin(mYaw) * cos(mPitch);
    mDirection.z = -sin(mPitch);

    mRight.x = sin(mYaw);
    mRight.y = -cos(mYaw);
    mRight.z = 0.0;

    mDirection = glm::normalize(mDirection);
    mRight = glm::normalize(mRight);
    mUp = glm::cross(mRight, mDirection);

    mDirtyView = true;
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
    mYaw = atan2(mDirection.y, mDirection.x);

    mDirtyView = true;
}

void Camera3D::copyFromSimpleCamera(SimpleCamera& simpleCamera) {
    mAspectRatio = simpleCamera.getAspectRatio();
    mPosition = simpleCamera.getPosition();
    mFieldOfView = simpleCamera.getFov();
    mZNear = simpleCamera.ZNEAR;
    mZFar = simpleCamera.ZFAR;
    mDirtyProjection = true;
    lookAt(mPosition + simpleCamera.getDirection());
    update();
}

void Camera3D::setOrientation(const f32q& orientation) {
    mDirection = orientation * f32v3(0.0, 0.0, 1.0);
    mRight = orientation * f32v3(1.0, 0.0, 0.0);
    mUp = orientation * f32v3(0.0, 1.0, 0.0);

    mPitch = asin(-mDirection.z);
    mYaw = atan2(mDirection.y, mDirection.x);

    mDirtyView = true;
}

f32v3 Camera3D::worldToScreenPoint(const f32v3& worldPoint) const {
    // Transform world to clipping coordinates
    f32v4 clipPoint = mMatrices.VP * f32v4(worldPoint - mPosition, 1.0f);
    clipPoint.x /= clipPoint.w;
    clipPoint.y /= clipPoint.w;
    clipPoint.z /= clipPoint.w;
    return f32v3((clipPoint.x + 1.0) / 2.0f,
        (1.0 - clipPoint.y) / 2.0f,
        (clipPoint.z + 1.0f) / 2.0f);
}

f32v3 Camera3D::getPickRayWorldSpace(f32v2 ndcScreenPos) const {
    f32v4 pickRayClipSpace(ndcScreenPos.x, ndcScreenPos.y, -1.0f, 1.0f);
    f32v4 pickRayEyeSpace = mMatrices.inverseP * pickRayClipSpace;
    pickRayEyeSpace.z = -1.0f;
    pickRayEyeSpace.w = 0.0f;
    f32v4 pickRayWorldSpace = mMatrices.inverseV * pickRayEyeSpace;
    f32v3 pickRayXYZ(pickRayWorldSpace.x, pickRayWorldSpace.y, pickRayWorldSpace.z);
    return glm::normalize(pickRayXYZ);
}
