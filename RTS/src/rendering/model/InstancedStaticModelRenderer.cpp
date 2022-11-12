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

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() {
    for (auto& it : mInstances) {
        glDeleteBuffers(1, &it.second.mInstanceVbo);
    }
}

void InstancedStaticModelRenderer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    StaticModelInstanceData& instanceData = mInstances[modelId];
    instanceData.mInstances.emplace_back(StaticModelInstance{ position, rotation });
    instanceData.mDirty = true;
}

void InstancedStaticModelRenderer::renderModels(const Camera3D& camera) {

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    VGUniform positionUniform = mStandardMaterial->getUniform("unTmpPosition");
    for (auto& it : mInstances) {
        StaticModelInstanceData& instanceData = it.second;
        // TODO: Culling
        if (instanceData.mDirty) {
            assert(false);
            if (instanceData.mInstanceVbo == 0) {
                glGenBuffers(1, &instanceData.mInstanceVbo);
            }
            glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(f32v2) * instanceData.mInstances.size(), &translations[0], GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribDivisor(2, 1);
        //ModelID modelId = it.first;
        //const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        //const Mesh& mesh = *model.getMesh();
        //for (auto&& instance : it.second) {
        //    glUniform3fv(positionUniform, 1, &instance.pos.x);
        //    //if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
        //    mesh.draw();
        //    //}
        //}
    }
    checkGlError("InstancedStaticModelRenderer::renderModels");
}

void InstancedStaticModelRenderer::addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer) {
    for (auto&& it : gatherer.mInstances) {
        const std::vector<StaticModelInstance>& sourceInstances = it.second;
        StaticModelInstanceData& instanceData = mInstances[it.first];
        instanceData.mInstances.reserve(instanceData.mInstances.size() + sourceInstances.size());
        instanceData.mInstances.insert(instanceData.mInstances.end(), sourceInstances.begin(), sourceInstances.end());
        instanceData.mDirty = true;
    }
}
