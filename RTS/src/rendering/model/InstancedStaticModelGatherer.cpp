#include "stdafx.h"
#include "InstancedStaticModelGatherer.h"

void InstancedStaticModelGatherer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    mInstances[modelId].emplace_back(StaticModelInstance{ position, rotation });
}
