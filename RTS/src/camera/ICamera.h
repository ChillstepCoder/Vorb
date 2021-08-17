#pragma once

class ICamera {
public:
    virtual const glm::mat4& getVPMatrix() const = 0;
    virtual f32 getScale() const = 0;
};