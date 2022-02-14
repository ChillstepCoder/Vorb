#include "stdafx.h"
#include "Chunk.h"

#include "rendering/QuadMesh.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "pathfinding/NavGraph.h"
#include "pathfinding/NavThread.h"
#include "world/WorldGrid.h"

#include "world/TileRepository.h"

#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"

bool IS_SHUTTING_DOWN = false;

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
    mTiles.resize(CHUNK_SIZE);
    mGrass.resize(CHUNK_SIZE);
}

void Chunk::freeTiles() {
    std::vector<Tile>().swap(mTiles);
    std::vector<ui8>().swap(mGrass);
}

void Chunk::dispose() {
    assert(IS_SHUTTING_DOWN || mRefCount.load() == 0);

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
    mChunkRenderData.mMeshDirty = true;
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
    return TileHandle(this, index);
}

TileHandle Chunk::getLeftTileHandle(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x > 0) {
        return TileHandle(this, index - 1);
    }
    const Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isDataReady()) {
        return TileHandle(&leftNeighbor, index + CHUNK_WIDTH - 1);
    }
	return TileHandle();
}

TileHandle Chunk::getRightTileHandle(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x < CHUNK_WIDTH - 1) {
        return TileHandle(this, index + 1);
    }

    const Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isDataReady()) {
        return TileHandle(&rightNeighbor, index - CHUNK_WIDTH + 1);
    }
    return TileHandle();
}

TileHandle Chunk::getTopTileHandle(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y < CHUNK_WIDTH - 1) {
        return TileHandle(this, index + CHUNK_WIDTH);
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isDataReady()) {
        return TileHandle(&topNeighbor, index + CHUNK_WIDTH - CHUNK_SIZE);
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTileHandle(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y > 0) {
        return TileHandle(this, index - CHUNK_WIDTH);
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isDataReady()) {
        return TileHandle(&bottomNeighbor, index - CHUNK_WIDTH + CHUNK_SIZE);
    }
	return TileHandle();
}

void Chunk::getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
			neighbors[(int)NeighborIndex8::BOTTOM] = *bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTileHandle(bottom.index);
            neighbors[(int)NeighborIndex8::BOTTOM_LEFT] = *bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTileHandle(bottom.index);
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
            neighbors[(int)NeighborIndex8::TOP] = *top.tile;
            TileHandle topLeft = top.chunk->getLeftTileHandle(top.index);
            neighbors[(int)NeighborIndex8::TOP_LEFT] = *topLeft.tile;
            TileHandle topRight = top.chunk->getRightTileHandle(top.index);
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
        mChunkRenderData.mMeshDirty = true;

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
                    TileIndex tileIndex(TileIndex(chunkRelPos.x, chunkRelPos.y));
                    Tile& tile = getMutableTileAt(tileIndex);
                    if (tile.getLayersMainThread()[TILE_LAYER_GROUND] == TILE_ID_NONE) {
                        // If we have no ground layer, then we just set base Z to ground height
                        setTileBaseZPosition(tileIndex, mWorldGrid->computeCenterHeightAtTile(mChunkId, tileIndex));
                    }
                    else {
                        // What happens here? What happens when we cover up the tile?
                    }
                }
            }
        }
    }

}

void Chunk::setTileAt(TileIndex i, Tile tile) {
    assert(i < CHUNK_SIZE);
    const bool readLocked = isReadLocked();
    Tile& oldTile = mTiles[i];
    if (readLocked && !oldTile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    TileFlags newFlags = TileFlags(oldTile.tileFlags | tile.tileFlags);
    oldTile = tile;
    oldTile.setTileFlags(newFlags, readLocked); // Union tile flags
    // Update collision
    updateTileCollisionAt(i, tile.topLayer, readLocked);
    // Only dirty nav graph and mesh if we actually updated data
    if (!readLocked) {
        dirtyNavGraph();
        dirtyMesh();
    }

}

bool Chunk::canAddTile(TileIndex i, const TileData& tileData) const {
    return mTiles[i].canAddTile(tileData);
}

void Chunk::addTile(TileIndex i, const TileData& tileData) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.addTile(tileData, readLocked);
    if (!readLocked) {
        dirtyMesh();
        if (tileData.layer != TILE_LAYER_MID) {
            dirtyNavGraph();
        }
    }
}

bool Chunk::tryAddTile(TileIndex i, const TileData& tileData) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        bool success = tile.tryAddTile(tileData, readLocked);
        if (success) {
            mTilesNeedingThreadSafeCopy.push_back(i);
        }
        return success;
    }
    else {
        return tile.tryAddTile(tileData, readLocked);
    }
}

void Chunk::setTileLayer(TileIndex i, TileLayer layer, TileID id) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileLayer(layer, id, readLocked);
    // Only top has collision
    if (layer == TileLayer::Top) {
        updateTileCollisionAt(i, tile.topLayer, readLocked);
    }
    if (layer != TileLayer::Mid) {
        // Top and bottom can change nav graph
        // TODO: Make this smarter
        if (!readLocked) {
            dirtyNavGraph();
        }
    }
    if (!readLocked) {
        dirtyMesh();
    }
}

void Chunk::setTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileFlag(flag, readLocked);
}

void Chunk::setTileFlags(TileIndex i, TileFlags flags) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileFlags(flags, readLocked);
}

void Chunk::clearTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileFlag(flag, readLocked);
}

void Chunk::clearTileFlags(TileIndex i) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileFlags(readLocked);
}

void Chunk::clearTileCollisionFlags(TileIndex i) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileCollisionFlags(readLocked);

}

void Chunk::setTilePathWeight(TileIndex i, ui8 weight) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setPathWeight(weight, readLocked);

}

void Chunk::setTileBaseZPosition(TileIndex i, f32 baseZPosition) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setBaseZPosition(baseZPosition, readLocked);
    if (!readLocked) {
        dirtyMesh();
    }
}

void Chunk::incRefNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().incRef();
    getLeftNeighbor().incRef();
    getRightNeighbor().incRef();
    getTopNeighbor().incRef();
}

void Chunk::decRefNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().decRef();
    getLeftNeighbor().decRef();
    getRightNeighbor().decRef();
    getTopNeighbor().decRef();
}

void Chunk::incReadLockNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().incReadLock();
    getLeftNeighbor().incReadLock();
    getRightNeighbor().incReadLock();
    getTopNeighbor().incReadLock();
}

void Chunk::decReadLockNeighbors4() const {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().decReadLock();
    getLeftNeighbor().decReadLock();
    getRightNeighbor().decReadLock();
    getTopNeighbor().decReadLock();
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

bool Chunk::isReadLocked() const {
    assert(IS_MAIN_THREAD());
    //const bool readLocked = mReadLockCount.load() > 0 || Services::NavThread::ref().isRunningPathfind();
    //if (readLocked) std::cout << "READ LOCK DETECTED\n";
    return mReadLockCount.load() > 0 || Services::NavThread::ref().isRunningPathfind();
}

void Chunk::updateTileCollisionAt(TileIndex i, TileID tileId, bool readLocked) {
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.updateCollision(readLocked);
}
