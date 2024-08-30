#include "stdafx.h"
#include "InstancedStaticModelGatherer.h"
#include "rendering/model/ModelUtil.h"

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, f32 rotation, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, rotation, scale);
    instance.variantIndex = variantIndex;
    instance.damageData = std::move(damageData);
    instance.scale = scale;
}

void InstancedStaticModelGatherer::addInstance(ModelID modelId, TileIndex tileIndex, const f32v3& position, const f32v3& normal, f32 rotation, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale) {
    StaticModelInstance& instance = mInstances[modelId].emplace_back();
    instance.tileIndex = tileIndex;
    instance.matrix = ModelUtil::computeTransformMatrixForModel(position, normal, rotation, scale);
    instance.variantIndex = variantIndex;
    instance.damageData = std::move(damageData);
    instance.scale = scale;
}
