#pragma once

#include "rendering/model/StaticModelInstanceTransform.h"

class InstancedStaticModelGatherer {
    friend class InstancedStaticModelRenderer;
public:
    InstancedStaticModelGatherer(TileContainerID containerID) : mContainerID(containerID) {};
    VORB_NON_COPYABLE_BUT_MOVABLE(InstancedStaticModelGatherer);

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void addInstance(ModelID modelId, const f32v3& position, const f32v3& normal, f32 rotation);
private:
    // TODO: Store tileIndex or position
    std::map<ModelID, std::vector<StaticModelInstanceTransform>> mInstances;
    TileContainerID mContainerID;
};

