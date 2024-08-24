#include "stdafx.h"
#include "FishRenderer.h"

#include "debugging/DebugRenderer.h"


#include "world/World.h"
#include "world/ecosystem/FishEcosystem.h"

#include "resources/ResourceManager.h"
#include "resources/FishRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "rendering/renderstate/RenderStateManager.h"
#include "rendering/mesh/MeshDrawer.h"

#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "rendering/gl/GLObjects.h"

constexpr int MAX_INSTANCES_PER_FRAME = 2000;
constexpr int INSTANCE_TRANSFORM_DATA_SIZE = sizeof(FishGPUData);
constexpr int INSTANCE_TRANSFORM_BUFFER_SIZE = INSTANCE_TRANSFORM_DATA_SIZE * MAX_INSTANCES_PER_FRAME * 3; // 3x our maximum size

#define DRAW_WATER_CELLS 0

FishRenderer::FishRenderer() {
    mFishShaderHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("fish"));
}

FishRenderer::~FishRenderer() {
    for (FishInstanceBatch& instanceData : mFishInstanceBatches) {
        if (instanceData.mInstanceDataBuffer) {
            glUnmapBuffer(instanceData.mInstanceDataBuffer);
            glDeleteBuffers(1, &instanceData.mInstanceDataBuffer);
        }
    }
}

void FishRenderer::renderFishEcosystem(const Camera3D& camera, const World& world) {
    PROFILE_FUNCTION();

    if (mFishShaderHandle) {
        mFishShader = mFishShaderHandle->tryGetLoadedAsset();
    }
    else {
        mFishShader = nullptr;
    }
    if (!mFishShader) return;

    BoundingSphere boundingSphere;
    boundingSphere.radius = CHUNK_DIAGONAL_RADIUS + 1.0f;
    boundingSphere.center.z = 0.0f;

    // Wait for the GPU to finish with this section of the buffer
    if (mFence[mFrameIndex] != 0) {
        while (glClientWaitSync(mFence[mFrameIndex], 0, GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED) {
            // Keep waiting
        }
        glDeleteSync(mFence[mFrameIndex]);
    }
    mInstanceCountsThisFrame.clear();
    mFishInstanceDataIndexThisFrame.clear();

    const FishChunkRenderStateMap& renderState = world.getFishEcosystem().getRenderStateManager().getRenderStateForRender();
    for (auto&& fishChunkIter : renderState) {
        // Draw active fish
        const FishRenderState& chunkRenderState = fishChunkIter.second;
        boundingSphere.center.x = chunkRenderState.mChunkCenter.x;
        boundingSphere.center.y = chunkRenderState.mChunkCenter.y;
        if (camera.sphereIsVisible(boundingSphere)) {
            for (auto&& fish : chunkRenderState.mFish) {
                addFishInstance(fish.mFishId, fish.pos, fish.yawPitch, fish.scale, fish.turn, fish.time);
            }
        }
    }
    ModelRepository& modelRepo = ModelRepository::get();
    MaterialRenderer::bindMaterialShaderForRender(*mFishShader);
    const int transformIndexStart = mFrameIndex * MAX_INSTANCES_PER_FRAME;
    glUniform1i(mFishShader->getUniform("unBufferOffset"), transformIndexStart);

    VGBuffer variantUniform = mFishShader->getUniform("unVariantIndex");

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());

    for (size_t i = 0; i < mInstanceCountsThisFrame.size(); ++i) {
        FishInstanceBatch& instanceData = mFishInstanceBatches[i];
        if (instanceData.mModelID != INVALID_MODEL_ID) {
            const ui32 instanceCount = mInstanceCountsThisFrame[i];
            glFlushMappedNamedBufferRange(instanceData.mInstanceDataBuffer, transformIndexStart * INSTANCE_TRANSFORM_DATA_SIZE, instanceCount * INSTANCE_TRANSFORM_DATA_SIZE);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, instanceData.mInstanceDataBuffer);
            // Only one mesh is currently supported for fish

            ModelBatchSubmeshDrawDataSpanKey submeshSpanKey = modelRepo.getDrawDataSpanKeyForModel(instanceData.mModelID);
            const ModelBatchSubmeshDrawData& drawData = modelRepo.getSubmeshDrawDataArrayForModel(submeshSpanKey)[0];
            const ModelBatch& modelBatch = modelRepo.getModelBatch(drawData.batchID);
            modelBatch.unbindCurrentAttribs(); // Fish renderer doesn't use these
            glBindVertexArray(modelBatch.getVao());

            // TODO: Handle different variants
            glUniform1ui(variantUniform, modelRepo.getVariantArrayIndexDataForModel(instanceData.mModelID).offset);

            // TODO: We need to do proper LOD for fish using setup similar to dynamic renderer
            const MeshLODDrawInfo& drawInfo = drawData.lodDrawInfo[0];

            // TODO: indirect
            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES,
                drawInfo.indexCount,
                (GLuint)modelBatch.getIndexType(),
                (const GLvoid*)(drawInfo.startIndex * modelBatch.getIndexSize()) /* offset */,
                instanceCount,
                drawData.baseVertex
            );
        }
    }

    //mIndirectBuffer->uploadIndirectBuffer();
    //MeshDrawer::drawIndirect(mesh.mMainMesh, instanceData.mShadowDrawCommandsCount, &drawCommands);


    // Create a new fence sync object for this frame
    mFence[mFrameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    incrementMod3(mFrameIndex);
}

void FishRenderer::debugRenderFishEcosystem(const World& world) {

    constexpr int DEBUG_LIFETIME = 0; // 40
    const f32 RENDER_DISTANCE_SQ = SQ(sDebugOptions.mFishRenderDistance);

    // Dont spam this every frame
    if (mDebugTickCounter-- == 0) {
        mDebugTickCounter = DEBUG_LIFETIME;
        const FishEcosystem& fishEcosystem = world.getFishEcosystem();
        // TODO: NOT THREAD SAFE
        AM::DebugRenderer::reserveFilledQuads(CHUNK_SIZE * fishEcosystem.mActiveFishChunks.size(), DEBUG_LIFETIME);
        for (auto&& it : fishEcosystem.mActiveFishChunks) {
            ChunkID id = it.first;
            const FishChunk& fishChunk = *it.second;
            const Chunk& chunk = world.getChunkGrid().getChunk(id);
            if (fishChunk.mInUpdateRange) {
                AM::DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::Cyan, DEBUG_LIFETIME);
            }
            else {
                AM::DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::OrangeRed, DEBUG_LIFETIME);
            }

            //// Draw active fish

            //for (int i = 0; i < 4; ++i) {
            //    if (glm::distance2(world.getLoadCenter(), fishChunk.getWorldCenterF()) < RENDER_DISTANCE_SQ) {
            //        for (auto&& fish : fishChunk.mFish) {
            //            DebugRenderer::drawFilledQuad(f32v3(fish.pos.x - 0.3f, fish.pos.y - 0.3f, 0.0f), f32v2(0.6f), color4(0, 255, 255, 128), 0);
            //        }
            //    }
            //}

#if DRAW_WATER_CELLS == 1
            const int CELL_ROW_STRIDE = FISH_CELLS_WIDTH * FISH_CELL_TILE_SIZE;
            for (int cy = 0; cy < FISH_CELLS_WIDTH; ++cy) {
                const int yIndexStart = cy * CELL_ROW_STRIDE;
                for (int cx = 0; cx < FISH_CELLS_WIDTH; ++cx) {
                    const FishCell& cell = fishChunk.mCells[cy * FISH_CELLS_WIDTH + cx];
                    const int indexStart = yIndexStart + cx * FISH_CELL_TILE_WIDTH;
                    if (cell.mTotalSpawnableTiles) {
                        for (int y = 0; y < FISH_CELL_TILE_WIDTH; ++y) {
                            for (int x = 0; x < FISH_CELL_TILE_WIDTH; ++x) {
                                if (cell.mSpawnableTiles.getBit(y * FISH_CELL_TILE_WIDTH + x)) {
                                    const TileIndex tileIndex = indexStart + y * CHUNK_WIDTH + x;
                                    const f32v3 pos = chunk.getTileContainer()->getTileSpatialGrid().getTileBaseWorldPos3D(tileIndex);
                                    AM::DebugRenderer::drawFilledQuad(pos, f32v2(1.0f), color::Green, DEBUG_LIFETIME);
                                }
                            }
                        }
                    }
                }
            }
#endif
        }
        for (auto&& it : fishEcosystem.mDormantFishChunks) {
            ChunkID id = it.first;
            const DormantFishChunk& fishChunk = it.second;
            const Chunk& chunk = world.getChunkGrid().getChunk(id);
            AM::DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::Azure, DEBUG_LIFETIME);
        }
    }
}

void FishRenderer::addFishInstance(AssetID fishId, f32v3 pos, f32v2 yawPitch, f32 scale, f32 turn, f32 time) {
    ui32 instanceDataIndex;

    FishInstanceBatch* instanceData = nullptr;
    auto&& it = mFishInstanceDataIndexThisFrame.find(fishId);
    if (it != mFishInstanceDataIndexThisFrame.end()) {
        instanceDataIndex = it->second;
        instanceData = &mFishInstanceBatches[instanceDataIndex];
    }
    else {
        instanceDataIndex = mInstanceCountsThisFrame.size();
        mFishInstanceDataIndexThisFrame[fishId] = instanceDataIndex;
        mInstanceCountsThisFrame.emplace_back(1);
        // Lazily allocate data
        // TODO: Eventually shrink this if needed!
        if (instanceDataIndex >= mFishInstanceBatches.size()) {
            mFishInstanceBatches.emplace_back();
            instanceData = &mFishInstanceBatches.back();
            glCreateBuffers(1, &instanceData->mInstanceDataBuffer);
            glNamedBufferStorage(instanceData->mInstanceDataBuffer, INSTANCE_TRANSFORM_BUFFER_SIZE, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
            instanceData->mMappedInstanceDataBuffer = (FishGPUData*)glMapNamedBufferRange(instanceData->mInstanceDataBuffer, 0, INSTANCE_TRANSFORM_BUFFER_SIZE,
                GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        }
        else {
            instanceData = &mFishInstanceBatches[instanceDataIndex];
        }
        instanceData->mHandle = FishRepository::get().getAssetHandle(fishId);
        instanceData->mModelID = instanceData->mHandle->getLoadedOrUnloadedAsset().mModelId;
    }

    if (mInstanceCountsThisFrame[instanceDataIndex] >= MAX_INSTANCES_PER_FRAME) {
        return;
    }
    const int transformBufferOffset = mFrameIndex * MAX_INSTANCES_PER_FRAME + mInstanceCountsThisFrame[instanceDataIndex];

    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mPosition = pos;
    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mYaw = yawPitch.x;
    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mPitch = yawPitch.y;
    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mScale = scale;
    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mTurn = turn;
    instanceData->mMappedInstanceDataBuffer[transformBufferOffset].mTime = time;

    ++mInstanceCountsThisFrame[instanceDataIndex];
}

FishInstanceBatch::FishInstanceBatch() = default;
FishInstanceBatch::~FishInstanceBatch() = default;
