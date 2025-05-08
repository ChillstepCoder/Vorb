#pragma once

#include "rendering/model/MaterialRenderPassType.h"

struct DynamicModelInstanceState {
    DynamicModelInstanceState() = default;
    DynamicModelInstanceState(glm::quat orientation, f64v3 position, ModelID modelId)
        : orientation(orientation), positionXY(position.x, position.y), positionZ(position.z), modelId(modelId) {}
    DynamicModelInstanceState(glm::quat orientation, f64v2 positionXY, f32 positionZ, ModelID modelId)
        : orientation(orientation), positionXY(positionXY), positionZ(positionZ), modelId(modelId) {}
    DynamicModelInstanceState(glm::quat orientation, f32v3 position, ModelID modelId) 
        : orientation(orientation), positionXY(position.x, position.y), positionZ(position.z), modelId(modelId) {}

    f32v3 getPositionLowPrecision() const {
        return f32v3(positionXY.x, positionXY.y, positionZ);
    }

    glm::quat orientation;
    f64v2 positionXY; // Need high precision
    f32 positionZ;
    ModelID modelId;
};

class DynamicModelInstanceStateContainer {
public:
    void clear();
    void reserve(size_t size) { mState.reserve(size); }
    void add(glm::quat orientation, f64v3 position, ModelID modelId);
    void add(glm::quat orientation, f32v3 position, ModelID modelId);
    
    const std::vector<DynamicModelInstanceState>& getVec() const { return mState; }
    ui32 getSubmeshCount(MaterialRenderPassType renderPass) const { return mSubmeshCounts[e_cast(renderPass)]; }

private:
    void onAddInternal(ModelID modelId);

    std::vector<DynamicModelInstanceState> mState;
    std::array<ui32, e_count(MaterialRenderPassType)> mSubmeshCounts;
};