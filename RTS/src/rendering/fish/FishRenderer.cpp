#include "stdafx.h"
#include "FishRenderer.h"

#include "debugging/DebugRenderer.h"

// For fish
#include "world/srv/SrvWorldInterface.h"

#include "world/IWorld.h"
#include "world/ecosystem/FishEcosystem.h"

#include "resources/ResourceManager.h"
#include "resources/FishRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"

#include "rendering/renderstate/RenderStateManager.h"
#include "rendering/mesh/MeshDrawer.h"

#include "camera/Camera3D.h"

#include "rendering/gl/GLObjects.h"

constexpr int MAX_INSTANCES_PER_FRAME = 2000;
constexpr int INSTANCE_TRANSFORM_DATA_SIZE = sizeof(FishInstanceTransform);
constexpr int INSTANCE_TRANSFORM_BUFFER_SIZE = INSTANCE_TRANSFORM_DATA_SIZE * MAX_INSTANCES_PER_FRAME * 3; // 3x our maximum size

#define DRAW_WATER_CELLS 0

FishRenderer::FishRenderer() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const std::vector<FishDef>& allFish = resourceManager.getFishRepository().getAllFish();
    mFishInstanceData.resize(allFish.size());

    mFishShader = resourceManager.getMaterialShaderManager().getMaterialShader("fish");

    // TODO: Only allocate what we need!!! Most fish will not be rendering!
    for (int i = 0; i < mFishInstanceData.size(); ++i) {
        FishInstanceData& instanceData = mFishInstanceData[i];
        const FishDef& fishDef = allFish[i];
        instanceData.mMesh = &resourceManager.getModelRepository().getModelDef(fishDef.mModel).getMesh(0);

        glCreateBuffers(1, &instanceData.mInstanceTransformBuffer);
        glNamedBufferStorage(instanceData.mInstanceTransformBuffer, INSTANCE_TRANSFORM_BUFFER_SIZE, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
        instanceData.mMappedTransformBuffer = (FishInstanceTransform*)glMapNamedBufferRange(instanceData.mInstanceTransformBuffer, 0, INSTANCE_TRANSFORM_BUFFER_SIZE,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    }
}

FishRenderer::~FishRenderer()
{
    for (int i = 0; i < mFishInstanceData.size(); ++i) {
        FishInstanceData& instanceData = mFishInstanceData[i];
        glUnmapBuffer(instanceData.mInstanceTransformBuffer);
        glDeleteBuffers(1, &instanceData.mInstanceTransformBuffer);
    }
}

void FishRenderer::renderFishEcosystem(const Camera3D& camera, const IWorld& world) {
    PROFILE_FUNCTION();
    const SrvWorldInterface* srvWorldInterface = dynamic_cast<const SrvWorldInterface*>(&world);

    if (!srvWorldInterface) {
        return;
    }


    BoundingSphere boundingSphere;
    boundingSphere.radius = sqrt(pow(FISH_CELL_TILE_WIDTH * 0.5f, 2.0f) * 3.0f);
    boundingSphere.center.z = 0.0f;

    // Wait for the GPU to finish with this section of the buffer
    if (mFence[mFrameIndex] != 0) {
        while (glClientWaitSync(mFence[mFrameIndex], 0, GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED) {
            // Keep waiting
        }
        glDeleteSync(mFence[mFrameIndex]);
    }

    mInstanceCountsThisFrame.resize(mFishInstanceData.size());
    std::fill(mInstanceCountsThisFrame.begin(), mInstanceCountsThisFrame.end(), 0);

    const FishRenderState& renderState = srvWorldInterface->getFishEcosystem().getRenderStateManager().getRenderStateForRender();
    for (auto&& cell : renderState.mActiveCells) {
        // Draw active fish
        boundingSphere.center.x = cell.mCellCenter.x;
        boundingSphere.center.y = cell.mCellCenter.y;
        if (camera.sphereIsVisible(boundingSphere)) {
            for (auto&& fish : cell.mFish) {
                addFishInstance(fish.mFishId, fish.mPosition, 0.0f);
                DebugRenderer::drawFilledQuad(fish.mPosition, f32v2(1.0f), color4(0, 255, 255, 128), 0);
            }
        }
    }

    MaterialRenderer::bindMaterialForRender(*mFishShader);
    const int transformIndexStart = mFrameIndex * MAX_INSTANCES_PER_FRAME;
    glUniform1i(mFishShader->getUniform("unBufferOffset"), transformIndexStart);
    for (size_t i = 0; i < mFishInstanceData.size(); ++i) {
        const ui32 instanceCount = mInstanceCountsThisFrame[i];
        if (instanceCount) {
            FishInstanceData& instanceData = mFishInstanceData[i];
            glFlushMappedNamedBufferRange(instanceData.mInstanceTransformBuffer, transformIndexStart * INSTANCE_TRANSFORM_DATA_SIZE, instanceCount * INSTANCE_TRANSFORM_DATA_SIZE);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, instanceData.mInstanceTransformBuffer);
            MeshDrawer::drawInstanced(instanceData.mMesh->mMainMesh, instanceCount);
        }
    }

    //mIndirectBuffer->uploadIndirectBuffer();
    //MeshDrawer::drawIndirect(mesh.mMainMesh, instanceData.mShadowDrawCommandsCount, &drawCommands);


    // Create a new fence sync object for this frame
    mFence[mFrameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    incrementMod3(mFrameIndex);
}

void FishRenderer::debugRenderFishEcosystem(const IWorld& world) {

    constexpr int DEBUG_LIFETIME = 4; // 40

    // Dont spam this every frame
    if (mDebugTickCounter-- == 0) {
        mDebugTickCounter = DEBUG_LIFETIME;
        const SrvWorldInterface* srvWorldInterface = dynamic_cast<const SrvWorldInterface*>(&world);
        if (srvWorldInterface) {
            const FishEcosystem& fishEcosystem = srvWorldInterface->getFishEcosystem();
            // TODO: NOT THREAD SAFE
            DebugRenderer::reserveFilledQuads(CHUNK_SIZE * fishEcosystem.mActiveFishChunks.size(), DEBUG_LIFETIME);
            for (auto&& it : fishEcosystem.mActiveFishChunks) {
                ChunkID id = it.first;
                const FishChunk& fishChunk = *it.second;
                const Chunk& chunk = world.getChunkGrid().getChunk(id);
                DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::LightGray, DEBUG_LIFETIME);
                DebugRenderer::drawLineBetweenPoints(
                    f32v3(chunk.getWorldPos3D()) + f32v3(HALF_CHUNK_WIDTH, 0.0f, 0.0f),
                    f32v3(chunk.getWorldPos3D()) + f32v3(HALF_CHUNK_WIDTH, CHUNK_WIDTH, 0.0f),
                    color::LightPink, DEBUG_LIFETIME);
                DebugRenderer::drawLineBetweenPoints(
                    f32v3(chunk.getWorldPos3D()) + f32v3(0.0f, HALF_CHUNK_WIDTH, 0.0f),
                    f32v3(chunk.getWorldPos3D()) + f32v3(CHUNK_WIDTH, HALF_CHUNK_WIDTH, 0.0f),
                    color::LightPink, DEBUG_LIFETIME);

                // Draw active fish
                for (int i = 0; i < 4; ++i) {
                    for (auto&& fish : fishChunk.mCells[i].mFish) {
                        DebugRenderer::drawFilledQuad(fish.mPosition, f32v2(1.0f), color::Cyan, DEBUG_LIFETIME);
                    }
                }

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
                                        DebugRenderer::drawFilledQuad(pos, f32v2(1.0f), color::Green, DEBUG_LIFETIME);
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
                DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::Azure, DEBUG_LIFETIME);
            }
        }
    }
}

void FishRenderer::addFishInstance(FishID fish, f32v3 pos, f32 yaw) {
    if (mInstanceCountsThisFrame[fish] >= MAX_INSTANCES_PER_FRAME) {
        return;
    }
    const int transformBufferOffset = mFrameIndex * MAX_INSTANCES_PER_FRAME + mInstanceCountsThisFrame[fish];

    FishInstanceData& instanceData = mFishInstanceData[fish];
    instanceData.mMappedTransformBuffer[transformBufferOffset].mPosition = pos;
    instanceData.mMappedTransformBuffer[transformBufferOffset].mYaw = yaw;

    ++mInstanceCountsThisFrame[fish];
}
