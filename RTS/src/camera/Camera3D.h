#pragma once
#include "ICamera.h"

#include <Vorb/graphics/Frustum.h>

class Camera3D : public ICamera
{
public:
    Camera3D();
    void init(float aspectRatio);
    void offsetPosition(const f32v3& offset);
    void update();
    void updateProjection();
    virtual void applyRotation(const f32q& rot);
    virtual void rotateFromMouseAbsoluteUp(float dx, float dy, float speed, bool clampVerticalRotation = false);
    virtual void rotateFromMouse(float dx, float dy, float speed);
    virtual void rollFromMouse(float dx, float speed);
    void lookAt(const f32v3& pos);

    // Frustum wrappers
    bool pointInFrustum(const f32v3& pos) const { return mFrustum.pointInFrustum(pos - mPosition); }
    bool sphereIsVisible(const f32v3& pos, float radius) const override { return mFrustum.sphereInFrustum(pos - mPosition, radius); }

    // Setters
    void setOrientation(const f32q& orientation);
    void setPosition(const f32v3& position) { mPosition = position; mViewChanged = true; }
    void setDirection(const f32v3& direction) { mDirection = direction; mViewChanged = true; }
    void setRight(const f32v3& right) { mRight = right; mViewChanged = true; }
    void setUp(const f32v3& up) { mUp = up; mViewChanged = true; }
    void setClippingPlane(float zNear, float zFar) { mZNear = zNear; mZFar = zFar; mProjectionChanged = true; }
    void setFieldOfView(float fieldOfView) { mFieldOfView = fieldOfView; mProjectionChanged = true; }
    void setAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; mProjectionChanged = true; }

    // Gets the position of a 3D point on the screen plane
    f32v3 worldToScreenPoint(const f32v3& worldPoint) const;
    f32v3 worldToScreenPointLogZ(const f32v3& worldPoint, f32 zFar) const;
    f32v3 getPickRay(const f32v2& ndcScreenPos) const;

    //getters
    const f32v3 getPosition() const override { return mPosition; }
    const f32v3& getDirection() const { return mDirection; }
    const f32v3& getRightVector() const override { return mRight; }
    const f32v3& getFrontVector() const override { return mDirection; }
    const f32v3& getUpVector() const override { return mUp; }
    f32 getZAngle() const override { return atan2f(mDirection.y, mDirection.x) + M_PIF; }
    f32 getZNear() const override { return mZNear; }
    f32 getZFar() const override { return mZFar; };

    const f32m4& getViewMatrix() const override { return mV; }
    const f32m4& getInverseViewMatrix() const override { return mInverseV; }
    const f32m4& getProjectionMatrix() const override { return mP; }
    const f32m4& getInverseProjectionMatrix() const override { return mInverseP; }
    const f32m4& getVPMatrix() const override { return mVP; }
    const f32m4& getInverseVPMatrix() const override { return mInverseVP; }

    const f32& getNearClip() const { return mZNear; }
    const f32& getFarClip() const { return mZFar; }
    const f32& getFieldOfView() const { return mFieldOfView; }
    const f32& getAspectRatio() const { return mAspectRatio; }

    const vg::Frustum& getFrustum() const { return mFrustum; }

    //. TODO: THis is broken!
    f32 getScale() const override { return 1.0f; }

protected:
    void updateView();

    f32 mZNear = 0.1f;
    f32 mZFar = 200000.0f;
    f32 mFieldOfView = 75.0f;
    f32 mAspectRatio = 4.0f / 3.0f;
    bool mViewChanged = true;
    bool mProjectionChanged = true;

    f32v3 mPosition = f32v3(0.0);
    f32v3 mDirection = f32v3(1.0f, 0.0f, 0.0f);
    f32v3 mRight = f32v3(0.0f, 0.0f, 1.0f);
    f32v3 mUp = f32v3(0.0f, 1.0f, 0.0f);

    // This must match layout of GlobalUbo.glsl for fast data store copy
    //  ****************
    f32m4 mV;
    f32m4 mInverseV;
    f32m4 mP;
    f32m4 mInverseP;
    f32m4 mVP;
    f32m4 mInverseVP;
    //  ****************

    vg::Frustum mFrustum; ///< For frustum culling
};
