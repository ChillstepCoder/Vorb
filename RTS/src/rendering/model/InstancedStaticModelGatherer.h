#pragma once

#include "rendering/model/StaticModelInstance.h"

class InstancedStaticModelGatherer {
    friend class InstancedStaticModelManager;
public:
    InstancedStaticModelGatherer(TileContainerID containerID, const f32v3& rootPosition) : mContainerID(containerID), mRootPosition(rootPosition) {};
    VORB_NON_COPYABLE_BUT_MOVABLE(InstancedStaticModelGatherer);

    void addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, f32 rotation);
    void addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, const f32v3& normal, f32 rotation);
private:
    // TODO: Store tileIndex or position
    std::map<ModelID, std::vector<StaticModelInstance>> mInstances;
    TileContainerID mContainerID;
    const f32v3& mRootPosition;
};

