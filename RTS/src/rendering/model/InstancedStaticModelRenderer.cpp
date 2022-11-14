#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "rendering/mesh/ModelMeshBuilder.h"
#include "options/DebugOptions.h"

#include "camera/Camera3D.h"

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
    if (sDebugOptions.mHideModels)
        return;

    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    for (auto& it : mInstances) {
        mInstancesToRender.clear();
        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();
        StaticModelInstanceData& instanceData = it.second;
        // TODO: Culling
        for (const StaticModelInstance& instance : instanceData.mInstances) {
            if (camera.sphereIsVisible(instance.pos, 10.0f)) {
                mInstancesToRender.push_back(instance);
            }
        }

        if (mInstancesToRender.size()) {
            //if (instanceData.mDirty) {
            if (instanceData.mInstanceVbo == 0) {
                glGenBuffers(1, &instanceData.mInstanceVbo);
            }
            glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
            GLsizei bufferSizeBytes = sizeof(StaticModelInstance) * mInstancesToRender.size();
            glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, &mInstancesToRender[0]);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            ModelMeshBuilder::updateInstanceDataForStaticModel(mesh, instanceData.mInstanceVbo);
            //instanceData.mDirty = false;
        //}


            mesh.drawInstanced(mInstancesToRender.size());
        }
    }
    // TODO: Material specific
    glEnable(GL_CULL_FACE);

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

ui32 InstancedStaticModelRenderer::getNumModels() const {
    ui32 numModels = 0;
    for (auto& it : mInstances) {
        numModels += it.second.mInstances.size();
    }
    return numModels;
}
