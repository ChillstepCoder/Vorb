#include "stdafx.h"
#include "Chunk.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/ChunkGrassQuadtree.h"
// TODO: Can we eliminate this?
#include "rendering/RenderThreadTasks.h"

#include "pathfinding/NavWorld.h"
#include "pathfinding/NavThread.h"
#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"

#include "resources/TileRepository.h"

#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"


Chunk::Chunk() {
}

Chunk::~Chunk() {
	dispose();
}

void Chunk::init(const ChunkID& chunkId) {
	assert(mState == e_cast(ChunkState::INVALID));
	mChunkId = chunkId;
    f32v2 worldPos = chunkId.getWorldPos();
    mAABB.x = worldPos.x;
    mAABB.y = worldPos.y;
    mAABB.z = -2.0f;
    mAABB.width = CHUNK_WIDTH;
    mAABB.depth = CHUNK_WIDTH;
    mAABB.height = 4.0f;
}

void Chunk::allocateTileContainer() {
    assert(!mTileContainer);
    const i32v2& worldPosInt2D = mChunkId.getWorldPosInt();
    const ui32v3 worldPosInt3D(worldPosInt2D.x, worldPosInt2D.y, 0u);
    mTileContainer = TileContainerRepository::getNewTileContainer(worldPosInt3D, ui32v3(CHUNK_WIDTH, CHUNK_WIDTH, 1), 1, true);
    mGrass.resize(CHUNK_SIZE);
    mStructures.resize(CHUNK_SIZE);
    assert(mTileContainer);
}

void Chunk::freeTiles() {
    if (mTileContainer) {
        TileContainerRepository::destroyTileContainer(mTileContainer);
        mTileContainer = nullptr;
    }
    std::vector<ui8>().swap(mGrass);
}

void Chunk::dispose() {

    mFlags = 0;
    mState = e_cast(ChunkState::INVALID);
    freeTiles();

    //mChunkRenderData.mBillboardMesh.reset();
    //// Make sure no funny business
    //// TOCO: Crashes on shutdown
    //if (mChunkRenderData.mGrassLod) assert(IS_SHUTTING_DOWN || !mChunkRenderData.mGrassLod->getRefCount());
    //mChunkRenderData.mGrassLod.reset();

}

void Chunk::updateMainThread() {
    assert(mTileContainer);

    // Structure thread safety
    // TODO: REEEEE
    if (mStructuresNeedingThreadSafeCopy.size()) {
        for (TileIndex& i : mStructuresNeedingThreadSafeCopy) {
            ChunkStructureVector& handle = mStructures[i];
            // mmmmm no
            assert(false);
            handle.copyThreadData();
        }
        mStructuresNeedingThreadSafeCopy.clear();
        // mDirtyNav = true; // TODO: Make this smarter
    }
}

TileHandle Chunk::getTileHandleAt(const TileIndex index) const {
    return TileHandle(mTileContainer, index);
}

TileHandle Chunk::getLeftTileHandle(const TileIndex index) const {
    assert(mTileContainer);
    const ui32v2 offset = mTileContainer->getTileXYOffset(index);
    if (offset.x > 0) {
        return TileHandle(mTileContainer, index - 1);
    }
    const Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isDataReady()) {
        return TileHandle(leftNeighbor.getTileContainer(), index + CHUNK_WIDTH - 1);
    }
	return TileHandle();
}

TileHandle Chunk::getRightTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer->getTileXYOffset(index);
    if (offset.x < CHUNK_WIDTH - 1) {
        return TileHandle(mTileContainer, index + 1);
    }

    const Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isDataReady()) {
        return TileHandle(rightNeighbor.getTileContainer(), index - CHUNK_WIDTH + 1);
    }
    return TileHandle();
}

TileHandle Chunk::getTopTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer->getTileXYOffset(index);
    if (offset.y < CHUNK_WIDTH - 1) {
        return TileHandle(mTileContainer, index + CHUNK_WIDTH);
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isDataReady()) {
        return TileHandle(topNeighbor.getTileContainer(), index + CHUNK_WIDTH - CHUNK_SIZE);
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer->getTileXYOffset(index);
    if (offset.y > 0) {
        return TileHandle(mTileContainer, index - CHUNK_WIDTH);
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isDataReady()) {
        return TileHandle(bottomNeighbor.getTileContainer(), index - CHUNK_WIDTH + CHUNK_SIZE);
    }
	return TileHandle();
}

void Chunk::getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
            Chunk& bottomChunk = sWorld->getChunk(ChunkID::fromWorldI32v2(bottom.getWorldPos2D()));
			neighbors[(int)NeighborIndex8::BOTTOM] = *bottom.tile;
			TileHandle bottomLeft = bottomChunk.getLeftTileHandle(bottom.tileIndex);
            neighbors[(int)NeighborIndex8::BOTTOM_LEFT] = *bottomLeft.tile;
            TileHandle bottomRight = bottomChunk.getRightTileHandle(bottom.tileIndex);
            neighbors[(int)NeighborIndex8::BOTTOM_RIGHT] = *bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex8::LEFT] = *getLeftTileHandle(index).tile;

    // Right
    neighbors[(int)NeighborIndex8::RIGHT] = *getRightTileHandle(index).tile;

    { // Top 3
        TileHandle top = getTopTileHandle(index);
        if (top.isValid()) {
            Chunk& topChunk = sWorld->getChunk(ChunkID::fromWorldI32v2(top.getWorldPos2D()));
            neighbors[(int)NeighborIndex8::TOP] = *top.tile;
            TileHandle topLeft = topChunk.getLeftTileHandle(top.tileIndex);
            neighbors[(int)NeighborIndex8::TOP_LEFT] = *topLeft.tile;
            TileHandle topRight = topChunk.getRightTileHandle(top.tileIndex);
            neighbors[(int)NeighborIndex8::TOP_RIGHT] = *topRight.tile;
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
    return sWorld->getChunk(mChunkId.id - 1);
}

Chunk& Chunk::getTopNeighbor() const {
    return sWorld->getChunk(mChunkId.id + WorldData::WORLD_WIDTH_CHUNKS);
}

Chunk& Chunk::getRightNeighbor() const {
    return sWorld->getChunk(mChunkId.id + 1);
}

Chunk& Chunk::getBottomNeighbor() const {
    return sWorld->getChunk(mChunkId.id - WorldData::WORLD_WIDTH_CHUNKS);
}

void Chunk::setGrassAt(const TileIndex index, ui8 grass) {
    mGrass[index] = grass;
    LOG_CRITICAL("Need to update Chunk::setGrassAt");
    // TODO: mark dirty
    /* if (mChunkRenderData.mGrassLod) {
         mChunkRenderData.mGrassLod
     }*/
}

void Chunk::setStructureAt(const TileIndex index, Structure* structure) {
    ChunkStructureVector& handle = mStructures[index];
    assert(structure != nullptr);
    handle.add(structure);
}


void Chunk::removeStructureAt(const TileIndex index, Structure* structure) {
    ChunkStructureVector& handle = mStructures[index];
    assert(structure != nullptr);
    handle.remove(structure);
}

StructureArrayPtr Chunk::getStructuresAt(const TileIndex index) const {
    assert(IS_GAME_THREAD());
    return mStructures[index].getMainThreadData();
}

StructureArrayPtr Chunk::getStructuresAtThreadSafe(const TileIndex index) const {
    assert(!IS_GAME_THREAD());
    return mStructures[index].getWorkerThreadData();
}

void Chunk::onTerrainDataChanged(const f32v2& editPosition, f32 editRadius) {
    constexpr ui32 DEBUG_DURATION = 100;
    const f32v2 dims = f32v2(CHUNK_WIDTH);
    const f32v2 halfDims = dims * 0.5f;
    const f32v2 worldPos = getWorldPos();
    const f32v2 offsetFromCenter = editPosition - (worldPos + halfDims);
    if (abs(offsetFromCenter.x) < halfDims.x + editRadius && abs(offsetFromCenter.y) < halfDims.y + editRadius) {

        // TODO: dirty grass

        // Update baseZ position
        const f32v2 startPos = editRadius - f32v2(editRadius);
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
                    TileIndex tileIndex = mTileContainer->getTileIndexFromXYZOffset(chunkRelPos.x, chunkRelPos.y, 0);
                    Tile& tile = mTileContainer->getMutableTileAt(tileIndex);
                    if (tile.getLayers()[TILE_LAYER_GROUND] == TILE_ID_NONE) {
                        // If we have no ground layer, then we just set base Z to ground height
                        mTileContainer->setTileGroundZPosition(tileIndex, sHeightmapGrid->computeCenterHeightAtTile(f32v2(chunkRelPos) + worldPos));
                    }
                    else {
                        // What happens here? What happens when we cover up the tile?
                    }
                }
            }
        }
    }

}
