#pragma once
#include "ICamera.h"

#include <Vorb/graphics/Frustum.h>

class SimpleCamera;

constexpr ui32 CAMERA_MATRICES_BYTE_SIZE = sizeof(f32m4) * 6 /*camera matrices*/;

class Camera3D : public ICamera
{
public:
    Camera3D();
    void applyRotation(const f32 pitch, const f32 yaw);
    virtual void applyRotation(const f32q& rot);
    virtual void rotateFromMouse(float dx, float dy, float speed);
    virtual void rollFromMouse(float dx, float speed);
    void lookAt(const f32v3& pos);
    void copyFromSimpleCamera(const SimpleCamera& simpleCamera);

    // Frustum wrappers
    bool pointInFrustum(const f32v3& pos) const { return mFrustum.pointInFrustum(pos - mPosition); }
    bool sphereIsVisible(const BoundingSphere& bounds) const { return mFrustum.sphereInFrustum(bounds.center - mPosition, bounds.radius); }
    bool sphereIsVisible(const f32v3& pos, float radius) const override { return mFrustum.sphereInFrustum(pos - mPosition, radius); }

    // Setters
    void setOrientation(const f32q& orientation);
    void setClippingPlane(float zNear, float zFar) { mZNear = zNear; mZFar = zFar; mDirtyProjection = true; }
    void setFieldOfView(float fieldOfView) { mFieldOfView = fieldOfView; mDirtyProjection = true; }

    // Gets the position of a 3D point on the screen plane
    f32v3 worldToScreenPoint(const f32v3& worldPoint) const;
    f32v3 getPickRayWorldSpace(f32v2 ndcScreenPos) const;

    //getters
    f32 getZAngle() const override { return atan2f(mDirection.y, mDirection.x) + M_PIF; }
    f32 getZNear() const override { return mZNear; }
    f32 getZFar() const override { return mZFar; };
    const f32 getYaw() const { return mYaw; }
    const f32 getPitch() const { return mPitch; }

    const f32& getNearClip() const { return mZNear; }
    const f32& getFarClip() const { return mZFar; }
    const f32& getFieldOfView() const { return mFieldOfView; }
    const f32& getAspectRatio() const { return mAspectRatio; }

    const vg::Frustum& getFrustum() const { return mFrustum; }

protected:
    void updateProjection() override;
    void postUpdate(bool changed) override;


    f32 mZNear = 0.1f;
    f32 mZFar = 200000.0f;
    f32 mFieldOfView = 75.0f;

    f32 mPitch = 0.0f;
    f32 mYaw = 0.0f; // Atomic because we use it on game thread for player update

    // See: CAMERA_MATRICES_BYTE_SIZE
    //  ****************
    // mVP in ICamera
    //  ****************

    vg::Frustum mFrustum; ///< For frustum culling
};
