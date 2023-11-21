#pragma once

// This must match layout of GlobalUbo.glsl for fast data store copy
struct CameraMatrices {
    f32m4 V;
    f32m4 inverseV;
    f32m4 P;
    f32m4 inverseP;
    f32m4 VP;
    f32m4 inverseVP;
};

class ICamera {
public:
    void update() {
        preUpdate();

        bool changed = false;
        if (mViewChanged) {
            updateView();
            mViewChanged = false;
            changed = true;
        }
        if (mProjectionChanged) {
            updateProjection();
            mProjectionChanged = false;
            changed = true;
        }
        if (changed) {
            updateVP();
        }

        postUpdate(changed);
    }
    void offsetPosition(const f32v3& offset) { mPosition += offset; mViewChanged = true; }
    void setPosition(const f32v3& position) { mPosition = position; mViewChanged = true; }
    void setDirection(const f32v3& direction) { mDirection = direction; mViewChanged = true; }
    void setRight(const f32v3& right) { mRight = right; mViewChanged = true; }
    void setUp(const f32v3& up) { mUp = up; mViewChanged = true; }
    void setAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; mProjectionChanged = true; }

    const f32m4& getViewMatrix() const { return mMatrices.V; }
    const f32m4& getInverseViewMatrix() const { return mMatrices.inverseV; }
    const f32m4& getProjectionMatrix() const { return mMatrices.P; }
    const f32m4& getInverseProjectionMatrix() const { return mMatrices.inverseP; }
    const f32m4& getVPMatrix() const { return mMatrices.VP; }
    const f32m4& getInverseVPMatrix() const { return mMatrices.inverseVP; }
    const CameraMatrices& getCameraMatrices() const { return mMatrices; }

    const f32v3 getPosition() const { return mPosition; }
    const f32v3 getDirection() const { return mDirection; }
    const f32v3 getRightVector() const { return mRight; }
    const f32v3 getFrontVector() const { return mDirection; }
    const f32v3 getUpVector() const  { return mUp; }
    virtual f32 getZAngle() const = 0;
    virtual f32 getZNear() const = 0;
    virtual f32 getZFar() const = 0;
    virtual bool sphereIsVisible(const f32v3& pos, float radius) const = 0;

protected:
    virtual void updateProjection() = 0;
    virtual void updateView() = 0;

    // Override to set custom update logic
    virtual void preUpdate() {}
    virtual void postUpdate(bool changed) { UNUSED(changed); }

    void updateVP() {
        mMatrices.VP = mMatrices.P * mMatrices.V;
        mMatrices.inverseVP = glm::inverse(mMatrices.VP);
    }

    // === Data ===
    bool mViewChanged = true;
    bool mProjectionChanged = true;
    CameraMatrices mMatrices;

    f32 mAspectRatio = 4.0f / 3.0f;
    f32v3 mPosition = f32v3(0.0);
    f32v3 mDirection = f32v3(1.0f, 0.0f, 0.0f);
    f32v3 mRight = f32v3(0.0f, 0.0f, 1.0f);
    f32v3 mUp = f32v3(0.0f, 1.0f, 0.0f);
};