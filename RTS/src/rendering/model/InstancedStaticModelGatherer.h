#pragma once

#include "rendering/model/StaticModelInstance.h"

class InstancedStaticModelGatherer {
    friend class InstancedStaticModelRenderer;
public:
    InstancedStaticModelGatherer() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(InstancedStaticModelGatherer);

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
private:
    std::map<ModelID, std::vector<StaticModelInstance>> mInstances;
};

