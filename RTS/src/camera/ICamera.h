#pragma once

class ICamera {
public:
    virtual const f32m4& getViewMatrix() const = 0;
    virtual const f32m4& getInverseViewMatrix() const = 0;
    virtual const f32m4& getProjectionMatrix() const = 0;
    virtual const f32m4& getInverseProjectionMatrix() const = 0;
    virtual const f32m4& getVPMatrix() const = 0;
    virtual const f32m4& getInverseVPMatrix() const = 0;

    virtual const f32v3& getRightVector() const = 0;
    virtual const f32v3& getFrontVector() const = 0;
    virtual const f32v3& getUpVector() const = 0;
    virtual const f32v3 getPosition() const = 0;
    virtual f32 getZAngle() const = 0;
    virtual f32 getZNear() const = 0;
    virtual f32 getZFar() const = 0;
    virtual bool sphereIsVisible(const f32v3& pos, float radius) const = 0;
};