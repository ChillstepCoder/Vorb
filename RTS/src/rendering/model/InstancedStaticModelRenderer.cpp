#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

InstancedStaticModelRenderer::InstancedStaticModelRenderer()
{

}

void InstancedStaticModelRenderer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    mInstances[modelId].emplace_back(StaticModelInstance{ position, rotation });
}

void InstancedStaticModelRenderer::renderModels(const Camera3D& camera) {

    PROFILE_FUNCTION();

    for (auto& it : mInstances) {
        ModelID modelId = it.first;
        for (auto&& instance : it.second) {

        }
    }
}
