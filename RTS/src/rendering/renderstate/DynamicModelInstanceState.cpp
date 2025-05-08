#include "stdafx.h"
#include "DynamicModelInstanceState.h"

#include "resources/ModelRepository.h"

void DynamicModelInstanceStateContainer::clear() {
    mState.clear();
    mSubmeshCounts.fill(0);
}


void DynamicModelInstanceStateContainer::add(glm::quat orientation, f64v3 position, ModelID modelId) {
    mState.emplace_back(orientation, position, modelId);
    onAddInternal(modelId);
}


void DynamicModelInstanceStateContainer::add(glm::quat orientation, f32v3 position, ModelID modelId) {
    mState.emplace_back(orientation, position, modelId);
    onAddInternal(modelId);
}

void DynamicModelInstanceStateContainer::onAddInternal(ModelID modelId)
{
    std::array<ui8, e_count(MaterialRenderPassType)>& submeshCounts = ModelRepository::get().getModelSubmeshCountsPerPass(modelId);

    // Only supports these
    mSubmeshCounts[e_cast(MaterialRenderPassType::Default)] += submeshCounts[e_cast(MaterialRenderPassType::Default)];
    mSubmeshCounts[e_cast(MaterialRenderPassType::Smudge)] += submeshCounts[e_cast(MaterialRenderPassType::Smudge)];
    static_assert(e_count(MaterialRenderPassType) == 3);
}
