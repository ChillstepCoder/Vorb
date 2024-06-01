#include "stdafx.h"
#include "Chunk.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/ChunkGrassQuadtree.h"
// TODO: Can we eliminate this?
#include "rendering/RenderThreadTasks.h"

#include "pathfinding/NavWorld.h"
#include "pathfinding/NavThread.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "building/Building.h"

#include "resources/TileRepository.h"
#include "tile/TileContainerRepository.h"

#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemDef.h"

#include "world/IChunkGrid.h"


Chunk::Chunk() {
    
}

Chunk::~Chunk() {
	dispose();
}

void Chunk::init(World& world, ChunkID chunkId, i32v2 worldPos) {
    mWorld = &world;
    mChunkId = chunkId;
    mAABB.x = worldPos.x;
    mAABB.y = worldPos.y;
    mAABB.z = -2;
    mAABB.width = CHUNK_WIDTH;
    mAABB.depth = CHUNK_WIDTH;
    // TODO: uhhh...
    mAABB.height = 4;
}

void Chunk::allocateData() {
    assert(!mTileContainer);
    // We should only allocate at the beginning of a load cycle
    // This should be set by chunk grid
    assert(mState == ChunkState::WAITING_SIM_RELEASE);
    {
        std::lock_guard lock(mTileContainerLifetimeMutex);
        mTileContainer = mWorld->getTileContainerRepository().allocateChunkContainer(mAABB.pos, this);
    }
    mGrass.resize(CHUNK_SIZE);

    assert(mTileContainer);
}

void Chunk::freeData() {
    if (mTileContainer) {
        std::lock_guard lock(mTileContainerLifetimeMutex);
        mTileContainer->getWorld().getTileContainerRepository().destroyTileContainer(mTileContainer);
        mTileContainer = nullptr;
    }
    {
        // Clear grass
        std::lock_guard lock(mSharedGrassMutex);
        std::vector<TileGrass>().swap(mGrass);
    }
}

void Chunk::dispose() {
    mFlags = 0;
    mState = ChunkState::DEACTIVATED;
    freeData();
}

const HeightmapPatchID Chunk::getHeightmapPatchID() const {
    return mWorld->getHeightmapGrid().getSpatialGrid2D().getIDAtWorldPos(mAABB.pos);
}

TileHandle Chunk::getTileHandleAt(const TileIndex index) const {
    return TileHandle(mTileContainer, index);
}

TileHandle Chunk::getLeftTileHandle(const TileIndex index) const {
    assert(mTileContainer);
    const i32v2 offset = mTileContainer->getTileSpatialGrid().getTileXYOffset(index);
    if (offset.x > 0) {
        return TileHandle(mTileContainer, index - 1);
    }
    const Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isActivated()) {
        return TileHandle(leftNeighbor.getTileContainer(), index + CHUNK_WIDTH - 1);
    }
	return TileHandle();
}

TileHandle Chunk::getRightTileHandle(const TileIndex index) const {
    const i32v2 offset = mTileContainer->getTileSpatialGrid().getTileXYOffset(index);
    if (offset.x < CHUNK_WIDTH - 1) {
        return TileHandle(mTileContainer, index + 1);
    }

    const Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isActivated()) {
        return TileHandle(rightNeighbor.getTileContainer(), index - CHUNK_WIDTH + 1);
    }
    return TileHandle();
}

TileHandle Chunk::getTopTileHandle(const TileIndex index) const {
    const i32v2 offset = mTileContainer->getTileSpatialGrid().getTileXYOffset(index);
    if (offset.y < CHUNK_WIDTH - 1) {
        return TileHandle(mTileContainer, index + CHUNK_WIDTH);
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isActivated()) {
        return TileHandle(topNeighbor.getTileContainer(), index + CHUNK_WIDTH - CHUNK_SIZE);
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTileHandle(const TileIndex index) const {
    const i32v2 offset = mTileContainer->getTileSpatialGrid().getTileXYOffset(index);
    if (offset.y > 0) {
        return TileHandle(mTileContainer, index - CHUNK_WIDTH);
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isActivated()) {
        return TileHandle(bottomNeighbor.getTileContainer(), index - CHUNK_WIDTH + CHUNK_SIZE);
    }
	return TileHandle();
}

void Chunk::getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

    IChunkGrid& chunkGrid = mWorld->getChunkGrid();

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
            Chunk& bottomChunk = chunkGrid.getChunkAtPosition(bottom.getWorldPos2D());
			neighbors[(int)NeighborIndex8::BOTTOM] = bottom.getTile();
			TileHandle bottomLeft = bottomChunk.getLeftTileHandle(bottom.tileIndex);
            neighbors[(int)NeighborIndex8::BOTTOM_LEFT] = bottomLeft.getTile();
            TileHandle bottomRight = bottomChunk.getRightTileHandle(bottom.tileIndex);
            neighbors[(int)NeighborIndex8::BOTTOM_RIGHT] = bottomRight.getTile();
		}
	}

	// Left
    neighbors[(int)NeighborIndex8::LEFT] = getLeftTileHandle(index).getTile();

    // Right
    neighbors[(int)NeighborIndex8::RIGHT] = getRightTileHandle(index).getTile();

    { // Top 3
        TileHandle top = getTopTileHandle(index);
        if (top.isValid()) {
            Chunk& topChunk = chunkGrid.getChunk(chunkGrid.getChunkIDFromWorldPos(top.getWorldPos2D()));
            neighbors[(int)NeighborIndex8::TOP] = top.getTile();
            TileHandle topLeft = topChunk.getLeftTileHandle(top.tileIndex);
            neighbors[(int)NeighborIndex8::TOP_LEFT] = topLeft.getTile();
            TileHandle topRight = topChunk.getRightTileHandle(top.tileIndex);
            neighbors[(int)NeighborIndex8::TOP_RIGHT] = topRight.getTile();
        }
    }

}

void Chunk::getTileNeighbors4(const TileIndex index, OUT TileHandle neighbors[4]) const {

    neighbors[(int)NeighborIndex4::BOTTOM] = getBottomTileHandle(index);
    neighbors[(int)NeighborIndex4::LEFT] = getLeftTileHandle(index);
    neighbors[(int)NeighborIndex4::RIGHT] = getRightTileHandle(index);
    neighbors[(int)NeighborIndex4::TOP] = getTopTileHandle(index);

}

Chunk& Chunk::getLeftNeighbor() const {
    return mWorld->getChunkGrid().getChunk(mChunkId - 1);
}

Chunk& Chunk::getTopNeighbor() const {
    IChunkGrid& chunkGrid = mWorld->getChunkGrid();
    return chunkGrid.getChunk(mChunkId + chunkGrid.getWidthChunks());
}

Chunk& Chunk::getRightNeighbor() const {
    return mWorld->getChunkGrid().getChunk(mChunkId + 1);
}

Chunk& Chunk::getBottomNeighbor() const {
    IChunkGrid& chunkGrid = mWorld->getChunkGrid();
    return chunkGrid.getChunk(mChunkId - chunkGrid.getWidthChunks());
}

void Chunk::setGrassAt(const TileIndex index, TileGrassID grassId, ui8 density) {
    TileGrass& grass = mGrass[index];
    int lowestDensityIndex = 0;
    int lowestDensity = INT32_MAX;
    for (int i = 0; i < MAX_GRASS_TYPES_PER_TILE; ++i) {
        if (grass.grassIDs[i] == grassId) {
            {
                std::lock_guard lock(mSharedGrassMutex);
                grass.densities[i] = density;
                if (density == 0) {
                    grass.grassIDs[i] = INVALID_TILE_GRASS_ID;
                }
            }
            // Prevent default case below
            density = 0;
            break;
        }
        else if (grass.densities[i] < lowestDensity) {
            // Keep track of lowest density for replace
            lowestDensity = grass.densities[i];
            lowestDensityIndex = i;
        }
    }
    // If we didn't set or clear a grass above, replace the one with the lowest density
    if (density != 0) {
        std::lock_guard lock(mSharedGrassMutex);
        grass.grassIDs[lowestDensityIndex] = grassId;
        grass.densities[lowestDensityIndex] = density;
    }

    ChunkEvent editEvent{ *this, ChunkEventType::GrassEdit, index };
    dispatchGrassEdit(editEvent);
}

void Chunk::clearGrassAt(const TileIndex index) {
    mGrass[index] = TileGrass();

    ChunkEvent editEvent{ *this, ChunkEventType::GrassEdit, index };
    dispatchGrassEdit(editEvent);
}

const ui8 Chunk::getGrassDensityAt(const TileIndex index, TileGrassID grassId) const
{
    ASSERT_GAME_THREAD();
    const TileGrass& grass = mGrass[index];
    return grass.getDensity(grassId);
}

bool Chunk::copyPaddedGrassDataWorkerThread(TileGrass outGrassData[PADDED_CHUNK_WIDTH][PADDED_CHUNK_WIDTH]) const {
    PROFILE_FUNCTION();
    assert(!IS_GAME_THREAD());
    { // Copy true data with lock
        std::shared_lock lock(mSharedGrassMutex);
        if (!mGrass.size()) {
            return false;
        }
        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            // Memcpy each row for maximum speed
            memcpy(&outGrassData[y + 1][1], &mGrass[y * CHUNK_WIDTH], sizeof(TileGrass) * CHUNK_WIDTH);
        }
    }
    // Pad edges with interior copies, no lock needed
    // We are just using for interpolation so its ok if we have some seams
    // Bottom left corner
    outGrassData[0][0] = outGrassData[1][1];
    // Bottom row
    memcpy(&outGrassData[0][1], &outGrassData[1][1], sizeof(TileGrass) * CHUNK_WIDTH);
    // Bottom right corner
    outGrassData[0][PADDED_CHUNK_WIDTH - 1] = outGrassData[1][PADDED_CHUNK_WIDTH - 2];
    // Left and right edges
    for (int y = 1; y < PADDED_CHUNK_WIDTH - 1; ++y) {
        outGrassData[y][0] = outGrassData[y][1];
        outGrassData[y][PADDED_CHUNK_WIDTH - 1] = outGrassData[y][PADDED_CHUNK_WIDTH - 2];
    }
    // Top left corner
    outGrassData[PADDED_CHUNK_WIDTH - 1][0] = outGrassData[PADDED_CHUNK_WIDTH - 2][1];
    // Top row
    memcpy(&outGrassData[PADDED_CHUNK_WIDTH - 1][1], &outGrassData[PADDED_CHUNK_WIDTH - 2][1], sizeof(TileGrass) * CHUNK_WIDTH);
    // Top right corner
    outGrassData[PADDED_CHUNK_WIDTH - 1][PADDED_CHUNK_WIDTH - 1] = outGrassData[PADDED_CHUNK_WIDTH - 2][PADDED_CHUNK_WIDTH - 2];
    return true;
}

void Chunk::onTerrainDataChanged(const f32v2& editPosition, f32 editRadius) {
    assert(mTileContainer);
    IHeightmapGrid& heightmapGrid = mWorld->getHeightmapGrid();

    constexpr ui32 DEBUG_DURATION = 100;
    const f32v2 dims = f32v2(CHUNK_WIDTH);
    const f32v2 halfDims = dims * 0.5f;
    const f32v2 worldPos = getWorldPos();
    const f32v2 offsetFromCenter = editPosition - (worldPos + halfDims);
    std::vector<std::pair<TileIndex, f32>> bulkEdit;
    if (abs(offsetFromCenter.x) < halfDims.x + editRadius && abs(offsetFromCenter.y) < halfDims.y + editRadius) {

        // TODO: dirty grass

        // Update baseZ position
        const f32v2 startPos = editPosition - f32v2(editRadius);
        f32v2 offsetFromChunk = startPos - worldPos;
        f32 rangeX = editRadius * 2.0f;
        f32 rangeY = editRadius * 2.0f;
        if (offsetFromChunk.x < 0.0f) {
            rangeX += offsetFromChunk.x;
            offsetFromChunk.x = 0.0f;
        }
        if (offsetFromChunk.y < 0.0f) {
            rangeY += offsetFromChunk.y;
            offsetFromChunk.y = 0.0f;
        }
        for (f32 y = 0.0f; y <= rangeY; ++y) {
            for (f32 x = 0.0f; x <= rangeX; ++x) {
                const ui32v2 chunkRelPos(offsetFromChunk.x + x, offsetFromChunk.y + y);
                if (chunkRelPos.x < CHUNK_WIDTH && chunkRelPos.y < CHUNK_WIDTH) {
                    TileIndex tileIndex = mTileContainer->getTileSpatialGrid().getTileIndexFromXYZOffset(chunkRelPos.x, chunkRelPos.y, 0);
                    bulkEdit.emplace_back(std::make_pair(tileIndex, heightmapGrid.computeCenterHeightAtTile<false>(f32v2(chunkRelPos) + worldPos)));
                    //if (tile.getLayers()[TILE_LAYER_GROUND] == TILE_ID_NONE) {
                        // If we have no ground layer, then we just set base Z to ground height
                        //mTileContainer->setTileGroundZPosition(tileIndex, sHeightmapGrid->computeMinHeightAtTile(f32v2(chunkRelPos) + worldPos));
                    //}
                    //else {
                        // What happens here? What happens when we cover up the tile?
                    //}
                }
            }
        }
        if (bulkEdit.size()) {
            mTileContainer->bulkSetTileGroundZPosition(bulkEdit.data(), bulkEdit.size());
        }
    }
}
