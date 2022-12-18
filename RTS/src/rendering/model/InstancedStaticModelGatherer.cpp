#include "stdafx.h"
#include "InstancedStaticModelGatherer.h"
#include "rendering/model/ModelUtil.h"

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, f32 rotation) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, rotation);
}

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, const f32v3& normal, f32 rotation) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, normal, rotation);
}
