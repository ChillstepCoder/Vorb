#include "stdafx.h"
#include "FishRenderer.h"

#include "debugging/DebugRenderer.h"

// For fish
#include "world/srv/SrvWorldInterface.h"

#include "world/IWorld.h"
#include "world/ecosystem/FishEcosystem.h"

#include "camera/Camera3D.h"


#define DRAW_WATER_CELLS 0

void FishRenderer::renderFishEcosystem(const Camera3D& camera, const IWorld& world) {
    PROFILE_FUNCTION();
    const SrvWorldInterface* srvWorldInterface = dynamic_cast<const SrvWorldInterface*>(&world);
    BoundingSphere boundingSphere;
    boundingSphere.radius = sqrt(pow(FISH_CELL_TILE_WIDTH * 0.5f, 2.0f) * 3.0f);
    boundingSphere.center.z = 0.0f;
    if (srvWorldInterface) {
        const FishEcosystem& fishEcosystem = srvWorldInterface->getFishEcosystem();
        for (auto&& it : fishEcosystem.mActiveFishChunks) {
            const FishChunk& fishChunk = *it.second;
            // Draw active fish

            for (int i = 0; i < 4; ++i) {
                const FishCell& cell = fishChunk.mCells[i];
                boundingSphere.center.x = cell.mWorldPos.x + FISH_CELL_HALF_TILE_WIDTH;
                boundingSphere.center.y = cell.mWorldPos.y + FISH_CELL_HALF_TILE_WIDTH;
                if (camera.sphereIsVisible(boundingSphere)) {
                    std::lock_guard lock(cell.mMutex);
                    for (auto&& fish : cell.mFish) {
                        DebugRenderer::drawFilledQuad(fish.mPosition, f32v2(1.0f), color::Cyan, 0);
                    }
                }
            }
        }
    }
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
