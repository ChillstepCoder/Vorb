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

constexpr int WORK_GROUP_SIZE = 64;
// TODO: Read about advanced gpu driven rendering https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf

struct GpuCullUniformData {
    f32v4 cameraPos;
    f32v4 frustumPlanes[4];
    MeshLODDrawInfo lodDrawInfos[4];
    float lodDistancesSQ[4];
    ui32 numShapesToCull;
};
static_assert(sizeof(MeshLODDrawInfo) == sizeof(ui32v2));

StaticModelInstanceData::StaticModelInstanceData()/* : mNumVisibleMeshesBuffer(sizeof(ui32), nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)*/
{
    // TODO: Investigate why, hardware? Driver? - Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
   /* mNumVisibleMeshesBufferPtr = (uint32_t*)glMapNamedBuffer(mNumVisibleMeshesBuffer.getHandle(), GL_READ_WRITE);
    assert(mNumVisibleMeshesBufferPtr);*/
}

StaticModelInstanceData::~StaticModelInstanceData()
{

}

InstancedStaticModelRenderer::InstancedStaticModelRenderer() :
    mGpuCullingUniformBuffer(sizeof(GpuCullUniformData), nullptr, GL_DYNAMIC_STORAGE_BIT) {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterialShader("standard_model");
    mShadowMapperMaterial = materialManager.getMaterialShader("shadow_mapper_instanced");
    mCullingComputeShader = materialManager.getComputeShader("culling_and_lod");
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() {
    for (auto& it : mInstances) {
        glDeleteBuffers(1, &it.second.mTransformsVbo);
    }
}

void InstancedStaticModelRenderer::frameUpdate(const Camera3D& camera) {
    assert(IS_RENDER_THREAD());
    if (sDebugOptions.mHideModels)
        return;

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

        if (instanceData.mFirstDirtyInstance != UINT32_MAX) {
            if (didUpdateACommandBuffer) {
                // If we dont have a valid command buffer yet, just skip this draw
                if (!instanceData.mDrawCommands) {
                    continue;
                }
            }
            else {
                didUpdateACommandBuffer = !sDebugOptions.mDisableGPUCulling; // disabled when cpu culling
                PROFILE_SCOPE("Rebuild Indirect Buffer");
                // TODO: Don't rebuild entire buffer?
                instanceData.mFirstDirtyInstance = UINT32_MAX;
                // Rebuild command buffer
                {
                    PROFILE_SCOPE("Indirect Buffer");
                    instanceData.mDrawCommands = std::make_unique<GLIndirectBuffer>(instanceData.mInstances.size());
                    for (size_t i = 0; i < instanceData.mDrawCommands->mDrawCommands.size(); ++i) {
                        DrawElementsIndirectCommand& cmd = instanceData.mDrawCommands->mDrawCommands[i];
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
            }
        }

        GLIndirectBuffer& inDrawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = inDrawCommands.mDrawCommands.size();


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
            //*instanceData.mNumVisibleMeshesBufferPtr = 0; // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync

            mCullingComputeShader->use();
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, mGpuCullingUniformBuffer.getHandle());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceData.mTransformsVbo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, inDrawCommands.getHandle());
            //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, instanceData.mNumVisibleMeshesBuffer.getHandle());
            //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, outDrawCommands.getHandle()); // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
            if (drawCommandsSize % WORK_GROUP_SIZE == 0) {
                glDispatchCompute((GLuint)drawCommandsSize / WORK_GROUP_SIZE, 1, 1);
            }
            else {
                glDispatchCompute(1 + (GLuint)drawCommandsSize / WORK_GROUP_SIZE, 1, 1);
            }
            glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT); // GL_ATOMIC_COUNTER_BARRIER_BIT
            // 114 fps

        }
        else {
            assert(drawCommandsSize == instanceData.mInstances.size());
            PROFILE_SCOPE("CPU Culling");
            // CPU Culling
            for (size_t i = 0; i < instanceData.mInstances.size(); ++i) {
                DrawElementsIndirectCommand& cmd = inDrawCommands.mDrawCommands[i];
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

            inDrawCommands.uploadIndirectBuffer();

        }

        // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
        // Sync start of draw
       /* if (inDrawCommands.mDrawCommands.size()) {
            glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
            instanceData.mFenceSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        }*/

    }

}

void InstancedStaticModelRenderer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    assert(IS_RENDER_THREAD());
    StaticModelInstanceData& instanceData = mInstances[modelId];
    instanceData.mInstances.emplace_back(StaticModelInstance{ glm::translate(glm::mat4(1.0f), position) });
    assert(false); // TODO: Support this
   // instanceData.mDirtyDrawCommands = true;
}

void InstancedStaticModelRenderer::removeInstanceAtPosition(TileContainerID containerId, const f32v3& position) {
    assert(IS_RENDER_THREAD());
}

void InstancedStaticModelRenderer::renderModels(const Camera3D& camera) {
    assert(IS_RENDER_THREAD());
    if (sDebugOptions.mHideModels)
        return;

    PROFILE_FUNCTION();

    // TODO: Material specific
    glDisable(GL_CULL_FACE);


    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    for (auto& it : mInstances) {
        StaticModelInstanceData& instanceData = it.second;
        if (!instanceData.mDrawCommands) {
            continue;
        }

        // Copy draw commands
        GLIndirectBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.mDrawCommands.size();
        if (!drawCommandsSize) {
            continue;
        }

        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();

        // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
        //// Make sure we created a fence for this instance
        //assert(instanceData.mFenceSync);
        //// Make sure all compute commands are finished
        //while (true) {
        //    const GLenum res = glClientWaitSync(instanceData.mFenceSync, GL_SYNC_FLUSH_COMMANDS_BIT, 100);
        //    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) break;
        //}
        //glDeleteSync(instanceData.mFenceSync);
        //instanceData.mFenceSync = 0;

        //const ui32 totalCommands = *instanceData.mNumVisibleMeshesBufferPtr;
        //assert(totalCommands == drawCommands.mDrawCommands.size());
        mesh.drawIndirect(drawCommands.mDrawCommands.size(), &drawCommands);
    }
    
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModels");
}

void InstancedStaticModelRenderer::renderModelShadows(const Camera3D& camera, const f32* shadowDistances) {
    assert(IS_RENDER_THREAD());
    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    for (auto& it : mInstances) {
        StaticModelInstanceData& instanceData = it.second;
        if (!instanceData.mDrawCommands) {
            continue;
        }

        // Copy draw commands
        GLIndirectBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.mDrawCommands.size();
        if (!drawCommandsSize) {
            continue;
        }

        ModelID modelId = it.first;
        const StaticModel3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).getStaticModel();
        const Mesh& mesh = *model.getMesh();

      
        mesh.drawIndirect(drawCommands.mDrawCommands.size(), &drawCommands);
    }

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelShadows");
}

void InstancedStaticModelRenderer::addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer) {
    assert(IS_RENDER_THREAD());
    if (gatherer.mInstances.empty()) {
        return;
    }
    // Gatherer should only be used once for init, and future updates should be done per tile
    assert(mTileContainerModels.find(gatherer.mContainerID) == mTileContainerModels.end());
    InstanceDataMap& tileContainerModels = mTileContainerModels[gatherer.mContainerID];
    for (auto&& it : gatherer.mInstances) {
        // Insert all instance transforms ordered into the transforms array
        const std::vector<StaticModelInstance>& sourceInstances = it.second;
        StaticModelInstanceData& instanceData = mInstances[it.first];
        const size_t startIndex = instanceData.mInstances.size();
        // Track where our buffer is dirty
        if (startIndex < instanceData.mFirstDirtyInstance) {
            instanceData.mFirstDirtyInstance = startIndex;
        }
        instanceData.mInstances.resize(startIndex + sourceInstances.size());
        // Store per tile references
        for (size_t i = 0; i < sourceInstances.size(); ++i) {
            size_t instanceIndex = startIndex + i;
            const StaticModelInstance& modelInstance = sourceInstances[i];
            instanceData.mInstances[instanceIndex] = modelInstance;
            const f32v3& pos = reinterpret_cast<const f32v3&>(modelInstance.matrix[3]);
            assert(tileContainerModels.find(pos) == tileContainerModels.end());
            tileContainerModels[pos] = { it.first, (ui32)instanceIndex };
        }
    }
}

void InstancedStaticModelRenderer::removeInstancesFromContainer(TileContainerID containerId)
{
    assert(IS_RENDER_THREAD());
    assert(false);
}

ui32 InstancedStaticModelRenderer::getNumModels() const {
    assert(IS_RENDER_THREAD());
    ui32 numModels = 0;
    for (auto& it : mInstances) {
        numModels += it.second.mInstances.size();
    }
    return numModels;
}