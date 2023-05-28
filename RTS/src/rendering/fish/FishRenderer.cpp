#include "stdafx.h"
#include "FishRenderer.h"

#include "debugging/DebugRenderer.h"

// For fish
#include "world/srv/SrvWorldInterface.h"

#include "world/IWorld.h"
#include "world/ecosystem/FishEcosystem.h"


#define DRAW_WATER_CELLS 0

void FishRenderer::debugRenderFishEcosystem(const IWorld& world) {

    constexpr int DEBUG_LIFETIME = 40;

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
                DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color::HotPink, DEBUG_LIFETIME);
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
