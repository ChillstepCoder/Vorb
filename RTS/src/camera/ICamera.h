#pragma once

class ICamera {
public:
    virtual const glm::mat4& getVPMatrix() const = 0;
    virtual const glm::mat4& getViewMatrix() const = 0;
    virtual f32 getScale() const = 0;
    virtual const f32v3& getRightVector() const = 0;
    virtual const f32v3& getFrontVector() const = 0;
    virtual const f32v3 getPosition() const = 0;
    virtual f32 getZAngle() const = 0;
    virtual bool sphereIsVisible(const f32v3& pos, float radius) const = 0;
};