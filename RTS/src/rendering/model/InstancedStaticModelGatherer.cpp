#include "stdafx.h"
#include "InstancedStaticModelGatherer.h"
#include "rendering/model/ModelUtil.h"

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, f32 rotation, ui8 variantIndex) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, rotation);
    instance.variantIndex = variantIndex;
}

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, const f32v3& normal, f32 rotation, ui8 variantIndex) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, normal, rotation);
    instance.variantIndex = variantIndex;
}
