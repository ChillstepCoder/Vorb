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

struct GpuCullUniformData {
    f32v4 cameraPos;
    f32v4 frustumPlanes[4];
    MeshLODDrawInfo lodDrawInfos[4];
    float lodDistancesSQ[4];
    ui32 numShapesToCull;
};
static_assert(sizeof(MeshLODDrawInfo) == sizeof(ui32v2));

StaticModelInstanceData::~StaticModelInstanceData()
{

}

InstancedStaticModelRenderer::InstancedStaticModelRenderer() : mGpuCullingUniformBuffer(sizeof(GpuCullUniformData), nullptr, GL_DYNAMIC_STORAGE_BIT) {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterial("standard_model");
    mShadowMapperMaterial = materialManager.getMaterial("shadow_mapper_instanced");
    mCullingComputeShader = materialManager.getComputeShader("culling_and_lod");
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() {
    for (auto& it : mInstances) {
        glDeleteBuffers(1, &it.second.mTransformsVbo);
        glDeleteBuffers(1, &it.second.mBoundingSpheresBuffer);
    }
}

void InstancedStaticModelRenderer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    StaticModelInstanceData& instanceData = mInstances[modelId];
    instanceData.mInstances.emplace_back(StaticModelInstance{ glm::translate(glm::mat4(1.0f), position) });
    instanceData.mDirtyDrawCommands = true;
}

void InstancedStaticModelRenderer::renderModels(const Camera3D& camera) {
    if (sDebugOptions.mHideModels)
        return;

    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    // Only do this once per frame because its expensive
    bool didUpdateACommandBuffer = false;
    
    for (auto& it : mInstances) {
        StaticModelInstanceData& instanceData = it.second;
        // TODO: Move this to onRemove
        if (!instanceData.mInstances.size()) {
            instanceData.mDrawCommands.reset();
            if (instanceData.mTransformsVbo) {
                glDeleteBuffers(1, &instanceData.mTransformsVbo);
                instanceData.mTransformsVbo = 0;
            }
            continue;
        }

        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();
        MeshLODDrawInfo drawInfos[4];
        for (int i = 0; i < 4; ++i) {
            drawInfos[i] = mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
        }

        if (instanceData.mDirtyDrawCommands) {
            if (didUpdateACommandBuffer) {
                // If we dont have a valid command buffer yet, just skip this draw
                if (!instanceData.mDrawCommands) {
                    continue;
                }
            }
            else {
                didUpdateACommandBuffer = !sDebugOptions.mDisableGPUCulling; // disabled when cpu culling
                PROFILE_SCOPE("Rebuild Indirect Buffer");
                instanceData.mDirtyDrawCommands = false;
                // Rebuild command buffer
                {
                    PROFILE_SCOPE("Indirect Buffer");
                    instanceData.mDrawCommands = std::make_unique<GLIndirectBuffer>(instanceData.mInstances.size());
                    for (size_t i = 0; i < instanceData.mDrawCommands->mDrawCommands.size(); ++i) {
                        DrawElementsIndirectCommand& cmd = instanceData.mDrawCommands->mDrawCommands[i];
                        cmd.count_ = mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(0)).indexCount; // TODO: FIX
                        cmd.firstIndex_ = 0;
                        cmd.baseVertex_ = 0;
                        cmd.baseInstance_ = i;
                    }
                }

                // Allocate VBO
                {
                    PROFILE_SCOPE("VBO");
                    GLsizei bufferSizeBytes = sizeof(StaticModelInstance) * instanceData.mInstances.size();
                    if (instanceData.mTransformsVbo == 0) {
                        glCreateBuffers(1, &instanceData.mTransformsVbo);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 7);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 8);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 9);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 10);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 7, 4, GL_FLOAT, GL_FALSE, 0);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 8, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 9, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2.0f);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 10, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3.0f);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 7, 1);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 8, 1);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 9, 1);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 10, 1);
                        glVertexArrayBindingDivisor(mesh.mMainMesh.mVao, 1, 1);
                        glNamedBufferStorage(instanceData.mTransformsVbo, bufferSizeBytes, instanceData.mInstances.data(), 0);
                        glVertexArrayVertexBuffer(mesh.mMainMesh.mVao, 1, instanceData.mTransformsVbo, 0, sizeof(StaticModelInstance));
                        instanceData.mTransformsVboSizeBytes = bufferSizeBytes;
                    }
                    else if (bufferSizeBytes > instanceData.mTransformsVboSizeBytes) {
                        // Grow to new size
                        glDeleteBuffers(1, &instanceData.mTransformsVbo);
                        glCreateBuffers(1, &instanceData.mTransformsVbo);
                        glNamedBufferStorage(instanceData.mTransformsVbo, bufferSizeBytes, instanceData.mInstances.data(), 0);
                        glVertexArrayVertexBuffer(mesh.mMainMesh.mVao, 1, instanceData.mTransformsVbo, 0, sizeof(StaticModelInstance));
                        instanceData.mTransformsVboSizeBytes = bufferSizeBytes;
                    }
                    else {
                        glNamedBufferSubData(instanceData.mTransformsVbo, 0, bufferSizeBytes, instanceData.mInstances.data());
                    }
                    instanceData.mDrawCommands->uploadIndirectBuffer();
                }

                // TODO: Real
                PROFILE_SCOPE("Bounding spheres");
                std::vector<BoundingSphere> boundingSpheres;
                boundingSpheres.resize(instanceData.mInstances.size());
                for (size_t i = 0; i < boundingSpheres.size(); ++i) {
                    boundingSpheres[i].center = reinterpret_cast<const f32v3&>(instanceData.mInstances[i].matrix[3]);
                    boundingSpheres[i].radius = 10.0f;
                }
                // Update bounding spheres
                glDeleteBuffers(1, &instanceData.mBoundingSpheresBuffer);
                glCreateBuffers(1, &instanceData.mBoundingSpheresBuffer);
                glNamedBufferStorage(instanceData.mBoundingSpheresBuffer, sizeof(BoundingSphere) * boundingSpheres.size(), boundingSpheres.data(), 0);
            }
        }

        GLIndirectBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.mDrawCommands.size();


        if (sDebugOptions.mDisableGPUCulling == false) {
            PROFILE_SCOPE("GPU Culling");
            // GPU Culling
            GpuCullUniformData uniformData;
            const f32v3& camPos = camera.getPosition();
            uniformData.cameraPos = f32v4(camPos.x, camPos.y, camPos.z, 1.0f);
            uniformData.numShapesToCull = drawCommandsSize;
            if (sDebugOptions.mDisableLOD) {
                uniformData.lodDistancesSQ[0] = FLT_MAX;
            }
            else {
                for (int i = 0; i < 3; ++i) {
                    uniformData.lodDistancesSQ[i] = SQ(sDebugOptions.mLodDistances[i]);
                }
            }
            for (int i = 0; i < 4; ++i) {
                uniformData.frustumPlanes[i] = camera.getFrustum().getPlane(i).vec4Data;
                uniformData.lodDrawInfos[i] = mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
            }
            mGpuCullingUniformBuffer.updateSubData(0, sizeof(GpuCullUniformData), &uniformData);

            mCullingComputeShader->use();
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, mGpuCullingUniformBuffer.getHandle());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceData.mBoundingSpheresBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, drawCommands.getHandle());
            //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, numVisibleMeshesBuffer.getHandle());
            if (drawCommandsSize % 64 == 0) {
                glDispatchCompute((GLuint)drawCommandsSize / 64, 1, 1);
            }
            else {
                glDispatchCompute(1 + (GLuint)drawCommandsSize / 64, 1, 1);
            }
            glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
            const GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

            MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
            mesh.drawIndirect(drawCommandsSize, &drawCommands);
        }
        else {
            assert(drawCommandsSize == instanceData.mInstances.size());
            PROFILE_SCOPE("CPU Culling");
            // CPU Culling
            for (size_t i = 0; i < instanceData.mInstances.size(); ++i) {
                DrawElementsIndirectCommand& cmd = drawCommands.mDrawCommands[i];
                StaticModelInstance& instance = instanceData.mInstances[i];
                // Columns are first
                const f32v3& pos = reinterpret_cast<const f32v3&>(instance.matrix[3]);
                if (camera.sphereIsVisible(pos, 10.0f)) {
                    cmd.instanceCount_ = 1;
                    MeshLODDrawInfo drawInfo;
                    f32 distance2 = glm::length2(pos - camera.getPosition());
                    if (distance2 < SQ(sDebugOptions.mLodDistances[0]) || sDebugOptions.mDisableLOD) {
                        drawInfo = drawInfos[0];
                    }
                    else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                        drawInfo = drawInfos[1];
                    }
                    else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                        drawInfo = drawInfos[2];
                    }
                    else {
                        drawInfo = drawInfos[3];
                    }
                    cmd.count_ = drawInfo.indexCount;
                    cmd.firstIndex_ = drawInfo.startIndex;
                }
                else {
                    cmd.instanceCount_ = 0;
                }
            }

            drawCommands.uploadIndirectBuffer();

            MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
            mesh.drawIndirect(drawCommands.mDrawCommands.size(), &drawCommands);
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
    assert(false);
    //MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    //for (auto& it : mInstances) {
    //    for (int i = 0; i < 4; ++i) {
    //        mInstancesToRender[i].clear();
    //    }
    //    ModelID modelId = it.first;
    //    const ModelDef& modelDef = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId);
    //    const StaticModel3D& model = modelDef.getStaticModel();
    //    const f32 maxDist = Shadows::getMaxDistance(shadowDistances, modelDef.mShadowDetail);
    //    const f32 maxDistSQ = SQ(maxDist);
    //    const Mesh& mesh = *model.getMesh();
    //    StaticModelInstanceData& instanceData = it.second;
    //    // TODO: Dont re-do culling for shadows
    //    for (const StaticModelInstance& instance : instanceData.mInstances) {
    //        if (camera.sphereIsVisible(instance.pos, 10.0f)) {
    //            f32v3 offset = instance.pos - camera.getPosition();
    //            f32 distance2 = glm::length2(offset);
    //            if (distance2 <= maxDistSQ) {
    //                if (distance2 < SQ(sDebugOptions.mLodDistances[0]) || sDebugOptions.mDisableLOD) {
    //                    mInstancesToRender[0].push_back(instance);
    //                }
    //                else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
    //                    mInstancesToRender[1].push_back(instance);
    //                }
    //                else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
    //                    mInstancesToRender[2].push_back(instance);
    //                }
    //                else {
    //                    mInstancesToRender[3].push_back(instance);
    //                }
    //            }
    //        }
    //    }

    //    for (int i = 0; i < 4; ++i) {
    //        if (mInstancesToRender[i].size()) {
    //            if (instanceData.mInstanceVbo == 0) {
    //                glGenBuffers(1, &instanceData.mInstanceVbo);
    //            }
    //            glBindBuffer(GL_ARRAY_BUFFER, instanceData.mInstanceVbo);
    //            GLsizei bufferSizeBytes = sizeof(StaticModelInstance) * mInstancesToRender[i].size();
    //            glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
    //            glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, &mInstancesToRender[i][0]);
    //            glBindBuffer(GL_ARRAY_BUFFER, 0);
    //            ModelMeshBuilder::updateInstanceDataForStaticModel(mesh, instanceData.mInstanceVbo);

    //            mesh.drawInstanced(MeshLODLevel(i), mInstancesToRender[i].size());
    //        }
    //    }
    //}

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
        instanceData.mDirtyDrawCommands = true;
    }
}

ui32 InstancedStaticModelRenderer::getNumModels() const {
    ui32 numModels = 0;
    for (auto& it : mInstances) {
        numModels += it.second.mInstances.size();
    }
    return numModels;
}