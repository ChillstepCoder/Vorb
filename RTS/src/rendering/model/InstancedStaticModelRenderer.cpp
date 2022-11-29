#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "rendering/post_process/ShadowLodDetail.h"
#include "rendering/mesh/ModelMeshBuilder.h"
#include "options/DebugOptions.h"

#include "camera/Camera3D.h"

InstancedStaticModelRenderer::InstancedStaticModelRenderer() {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterial("standard_model");
    mShadowMapperMaterial = materialManager.getMaterial("shadow_mapper_instanced");
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
        for (int i = 0; i < 4; ++i) {
            mInstancesToRender[i].clear();
        }
        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();
        StaticModelInstanceData& instanceData = it.second;
        // TODO: Culling
        for (const StaticModelInstance& instance : instanceData.mInstances) {
            if (camera.sphereIsVisible(instance.pos, 10.0f)) {
                f32 distance2 = glm::length2(instance.pos - camera.getPosition());
                if (distance2 < SQ(sDebugOptions.mLodDistances[0]) || sDebugOptions.mDisableLOD) {
                    mInstancesToRender[0].push_back(instance);
                } else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                    mInstancesToRender[1].push_back(instance);
                }
                else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                    mInstancesToRender[2].push_back(instance);
                }
                else {
                    mInstancesToRender[3].push_back(instance);
                }
            }
        }

        for (int i = 0; i < 4; ++i) {
            if (mInstancesToRender[i].size()) {
                if (instanceData.mInstanceVbo == 0) {
                    glGenBuffers(1, &instanceData.mInstanceVbo);
                }
                glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
                GLsizei bufferSizeBytes = sizeof(StaticModelInstance) * mInstancesToRender[i].size();
                glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
                glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, &mInstancesToRender[i][0]);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                ModelMeshBuilder::updateInstanceDataForStaticModel(mesh, instanceData.mInstanceVbo);

                mesh.drawInstanced(MeshLODLevel(i), mInstancesToRender[i].size());
            }
        }
    }
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModels");
}

void InstancedStaticModelRenderer::renderModelShadows(const Camera3D& camera, const f32* shadowDistances)
{
    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    for (auto& it : mInstances) {
        for (int i = 0; i < 4; ++i) {
            mInstancesToRender[i].clear();
        }
        ModelID modelId = it.first;
        const ModelDef& modelDef = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId);
        const StaticModel3D& model = modelDef.getStaticModel();
        const f32 maxDist = Shadows::getMaxDistance(shadowDistances, modelDef.mShadowDetail);
        const f32 maxDistSQ = SQ(maxDist);
        const Mesh& mesh = *model.getMesh();
        StaticModelInstanceData& instanceData = it.second;
        // TODO: Dont re-do culling for shadows
        for (const StaticModelInstance& instance : instanceData.mInstances) {
            if (camera.sphereIsVisible(instance.pos, 10.0f)) {
                f32v3 offset = instance.pos - camera.getPosition();
                f32 distance2 = glm::length2(offset);
                if (distance2 <= maxDistSQ) {
                    if (distance2 < SQ(sDebugOptions.mLodDistances[0]) || sDebugOptions.mDisableLOD) {
                        mInstancesToRender[0].push_back(instance);
                    }
                    else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                        mInstancesToRender[1].push_back(instance);
                    }
                    else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                        mInstancesToRender[2].push_back(instance);
                    }
                    else {
                        mInstancesToRender[3].push_back(instance);
                    }
                }
            }
        }

        for (int i = 0; i < 4; ++i) {
            if (mInstancesToRender[i].size()) {
                if (instanceData.mInstanceVbo == 0) {
                    glGenBuffers(1, &instanceData.mInstanceVbo);
                }
                glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
                GLsizei bufferSizeBytes = sizeof(StaticModelInstance) * mInstancesToRender[i].size();
                glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
                glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, &mInstancesToRender[i][0]);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                ModelMeshBuilder::updateInstanceDataForStaticModel(mesh, instanceData.mInstanceVbo);

                mesh.drawInstanced(MeshLODLevel(i), mInstancesToRender[i].size());
            }
        }
    }

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelShadows");
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
