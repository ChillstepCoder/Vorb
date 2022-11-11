#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

InstancedStaticModelRenderer::InstancedStaticModelRenderer() {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterial("standard_model");
}

void InstancedStaticModelRenderer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    mInstances[modelId].emplace_back(StaticModelInstance{ position, rotation });
}

void InstancedStaticModelRenderer::renderModels(const Camera3D& camera) {

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    VGUniform positionUniform = mStandardMaterial->getUniform("unPosition");
    for (auto& it : mInstances) {
        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();
        for (auto&& instance : it.second) {
            glUniform3fv(positionUniform, 1, &instance.pos.x);
            //if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            mesh.draw();
            //}
        }
    }
    checkGlError("InstancedStaticModelRenderer::renderModels");
}

void InstancedStaticModelRenderer::addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer) {
    for (auto&& it : gatherer.mInstances) {
        const std::vector<StaticModelInstance>& sourceInstances = it.second;
        std::vector<StaticModelInstance>& targetInstances = mInstances[it.first];
        targetInstances.reserve(targetInstances.size() + sourceInstances.size());
        targetInstances.insert(targetInstances.end(), sourceInstances.begin(), sourceInstances.end());
    }
}
