#include "stdafx.h"
#include "Chunk.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/QuadMesh.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "pathfinding/NavGraph.h"
#include "pathfinding/NavThread.h"
#include "world/WorldGrid.h"

#include "resources/TileRepository.h"

#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"

ChunkRenderData::~ChunkRenderData() {
    // Empty
}

Chunk::Chunk() {
}

Chunk::~Chunk() {
	dispose();
}

void Chunk::init(const ChunkID& chunkId, WorldGrid& worldGrid) {
    mWorldGrid = &worldGrid;
	assert(mState == e_cast(ChunkState::INVALID));
	mChunkId = chunkId;
    mWorldPos = chunkId.getWorldPos();
    mAABB.x = mWorldPos.x;
    mAABB.y = mWorldPos.y;
    mAABB.z = -2.0f;
    mAABB.width = CHUNK_WIDTH;
    mAABB.depth = CHUNK_WIDTH;
    mAABB.height = 4.0f;
}

void Chunk::allocateTiles() {
    // TODO: Not always
    mGrass.resize(CHUNK_SIZE);
    mStructures.resize(CHUNK_SIZE, nullptr);
}

void Chunk::freeTiles() {
    mTileContainer.freeTiles();
    std::vector<ui8>().swap(mGrass);
}

void Chunk::dispose() {
    assert(IS_SHUTTING_DOWN || mTileContainer.getRefCount() == 0);

    onDispose(this);
    if (isDataReady()) {
        Chunk& bottomNeighbor = getBottomNeighbor();
        if (bottomNeighbor.isDataReady()) {
            --bottomNeighbor.mDataReadyNeighborCount;
        }
        Chunk& leftNeighbor = getLeftNeighbor();
        if (leftNeighbor.isDataReady()) {
            --leftNeighbor.mDataReadyNeighborCount;
        }
        Chunk& rightNeighbor = getRightNeighbor();
        if (rightNeighbor.isDataReady()) {
            --rightNeighbor.mDataReadyNeighborCount;
        }
        Chunk& topNeighbor = getTopNeighbor();
        if (topNeighbor.isDataReady()) {
            --topNeighbor.mDataReadyNeighborCount;
        }
    }

    mState = e_cast(ChunkState::INVALID);

    mDataReadyNeighborCount = 0;
    
    // Reset render data
    mTileContainer.setDirtyMesh(true);
    mChunkRenderData.mIsVisible = false;

    mChunkRenderData.mBillboardMesh.reset();
    mChunkRenderData.mChunkMesh.reset();
    // Make sure no funny business
    // TOCO: Crashes on shutdown
    if (mChunkRenderData.mGrassLod) assert(!mChunkRenderData.mGrassLod->getRefCount());
    mChunkRenderData.mGrassLod.reset();

    freeTiles();
}

TileHandle Chunk::getTileHandleAt(const TileIndex index) const {
    return TileHandle(&mTileContainer, index);
}

TileHandle Chunk::getLeftTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer.getTileXYOffset(index);
    if (offset.x > 0) {
        return TileHandle(&mTileContainer, index - 1);
    }
    const Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isDataReady()) {
        return TileHandle(&leftNeighbor.getTileContainer(), index + CHUNK_WIDTH - 1);
    }
	return TileHandle();
}

TileHandle Chunk::getRightTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer.getTileXYOffset(index);
    if (offset.x < CHUNK_WIDTH - 1) {
        return TileHandle(&mTileContainer, index + 1);
    }

    const Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isDataReady()) {
        return TileHandle(&rightNeighbor.getTileContainer(), index - CHUNK_WIDTH + 1);
    }
    return TileHandle();
}

TileHandle Chunk::getTopTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer.getTileXYOffset(index);
    if (offset.y < CHUNK_WIDTH - 1) {
        return TileHandle(&mTileContainer, index + CHUNK_WIDTH);
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isDataReady()) {
        return TileHandle(&topNeighbor.getTileContainer(), index + CHUNK_WIDTH - CHUNK_SIZE);
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTileHandle(const TileIndex index) const {
    const ui32v2 offset = mTileContainer.getTileXYOffset(index);
    if (offset.y > 0) {
        return TileHandle(&mTileContainer, index - CHUNK_WIDTH);
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isDataReady()) {
        return TileHandle(&bottomNeighbor.getTileContainer(), index - CHUNK_WIDTH + CHUNK_SIZE);
    }
	return TileHandle();
}

void Chunk::getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
            Chunk& bottomChunk = mWorldGrid->getChunk(ChunkID::fromWorldUI32v2(bottom.getWorldPos2D()));
			neighbors[(int)NeighborIndex8::BOTTOM] = *bottom.tile;
			TileHandle bottomLeft = bottomChunk.getLeftTileHandle(bottom.index);
            neighbors[(int)NeighborIndex8::BOTTOM_LEFT] = *bottomLeft.tile;
            TileHandle bottomRight = bottomChunk.getRightTileHandle(bottom.index);
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
            Chunk& topChunk = mWorldGrid->getChunk(ChunkID::fromWorldUI32v2(top.getWorldPos2D()));
            neighbors[(int)NeighborIndex8::TOP] = *top.tile;
            TileHandle topLeft = topChunk.getLeftTileHandle(top.index);
            neighbors[(int)NeighborIndex8::TOP_LEFT] = *topLeft.tile;
            TileHandle topRight = topChunk.getRightTileHandle(top.index);
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
    return mWorldGrid->getChunk(mChunkId.id - 1);
}

Chunk& Chunk::getTopNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id + WorldData::WORLD_WIDTH_CHUNKS);
}

Chunk& Chunk::getRightNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id + 1);
}

Chunk& Chunk::getBottomNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id - WorldData::WORLD_WIDTH_CHUNKS);
}

void Chunk::setGrassAt(const TileIndex index, ui8 grass) {
    mGrass[index] = grass;
    // TODO: mark dirty
    /* if (mChunkRenderData.mGrassLod) {
         mChunkRenderData.mGrassLod
     }*/
}

void Chunk::onTerrainDataChanged(const f32v2& editPosition, f32 editRadius) {
    constexpr ui32 DEBUG_DURATION = 100;
    const f32v2 dims = f32v2(CHUNK_WIDTH);
    const f32v2 halfDims = dims * 0.5f;
    const f32v2 offsetFromCenter = editPosition - (mWorldPos + halfDims);
    if (abs(offsetFromCenter.x) < halfDims.x + editRadius && abs(offsetFromCenter.y) < halfDims.y + editRadius) {
        // This chunk is touched, mark meshes as dirty and pass on
        if (mChunkRenderData.mGrassLod) {
            mChunkRenderData.mGrassLod->onDataChanged(editPosition, editRadius);
        }
        mTileContainer.setDirtyMesh(true);

        // Update baseZ position
        const f32v2 startPos = editRadius - f32v2(editRadius);
        f32v2 offsetFromChunk = startPos - mWorldPos;
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
                    TileIndex tileIndex = mTileContainer.getTileIndexFromXYZOffset(chunkRelPos.x, chunkRelPos.y, 0);
                    Tile& tile = mTileContainer.getMutableTileAt(tileIndex);
                    if (tile.getLayersMainThread()[TILE_LAYER_GROUND] == TILE_ID_NONE) {
                        // If we have no ground layer, then we just set base Z to ground height
                        mTileContainer.setTileBaseZPosition(tileIndex, mWorldGrid->computeCenterHeightAtTile(f32v2(chunkRelPos) + mWorldPos));
                    }
                    else {
                        // What happens here? What happens when we cover up the tile?
                    }
                }
            }
        }
    }

}

void Chunk::incRefNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().getTileContainer().incRef();
    getLeftNeighbor().getTileContainer().incRef();
    getRightNeighbor().getTileContainer().incRef();
    getTopNeighbor().getTileContainer().incRef();
}

void Chunk::decRefNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().getTileContainer().decRef();
    getLeftNeighbor().getTileContainer().decRef();
    getRightNeighbor().getTileContainer().decRef();
    getTopNeighbor().getTileContainer().decRef();
}

void Chunk::incReadLockNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().getTileContainer().incReadLock();
    getLeftNeighbor().getTileContainer().incReadLock();
    getRightNeighbor().getTileContainer().incReadLock();
    getTopNeighbor().getTileContainer().incReadLock();
}

void Chunk::decReadLockNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().getTileContainer().decReadLock();
    getLeftNeighbor().getTileContainer().decReadLock();
    getRightNeighbor().getTileContainer().decReadLock();
    getTopNeighbor().getTileContainer().decReadLock();
}

void Chunk::incReadLockAndRefCountNeighbors4AndSelf() const {
    assert(mDataReadyNeighborCount == 4);
    incReadLockAndRefCount();
    getBottomNeighbor().incReadLockAndRefCount();
    getLeftNeighbor().incReadLockAndRefCount();
    getRightNeighbor().incReadLockAndRefCount();
    getTopNeighbor().incReadLockAndRefCount();
}

void Chunk::decReadLockAndRefCountNeighbors4AndSelf() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().decReadLockAndRefCount();
    getLeftNeighbor().decReadLockAndRefCount();
    getRightNeighbor().decReadLockAndRefCount();
    getTopNeighbor().decReadLockAndRefCount();
    decReadLockAndRefCount();
}
