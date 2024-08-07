#pragma once

struct DynamicModelInstanceState {
    DynamicModelInstanceState() = default;
    DynamicModelInstanceState(const glm::quat& orientation, f64v2 positionXY, f32 positionZ, ModelID modelId)
        : orientation(orientation), positionXY(positionXY), positionZ(positionZ), modelId(modelId) {}
    DynamicModelInstanceState(const glm::quat& orientation, f32v3 position, ModelID modelId) 
        : orientation(orientation), positionXY(position.x, position.y), positionZ(position.z), modelId(modelId) {}

    f32v3 getPositionLowPrecision() const {
        return f32v3(positionXY.x, positionXY.y, positionZ);
    }

    glm::quat orientation;
    f64v2 positionXY; // Need high precision
    f32 positionZ;
    ModelID modelId;
};
