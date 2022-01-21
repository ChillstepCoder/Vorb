#include "stdafx.h"
#include "Chunk.h"

#include "rendering/QuadMesh.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "pathfinding/NavGraph.h"
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
	assert(mState == ChunkState::INVALID);
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
        Chunk& bottomNeighbor = getBottomNeighbor();
        if (bottomNeighbor.isDataReady()) {
            --bottomNeighbor.mDataReadyNeighborCount;
        }
    }
    mDataReadyNeighborCount = 0;
	mState = ChunkState::INVALID;
    
    // Reset render data
    mChunkRenderData.mMeshDirty = true;
    mChunkRenderData.mIsVisible = false;

    mChunkRenderData.mBillboardMesh.reset();
    mChunkRenderData.mChunkMesh.reset();
    // Make sure no funny business
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
			neighbors[(int)NeighborIndex8::BOTTOM] = bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTileHandle(bottom.index);
            neighbors[(int)NeighborIndex8::BOTTOM_LEFT] = bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTileHandle(bottom.index);
            neighbors[(int)NeighborIndex8::BOTTOM_RIGHT] = bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex8::LEFT] = getLeftTileHandle(index).tile;

    // Right
    neighbors[(int)NeighborIndex8::RIGHT] = getRightTileHandle(index).tile;

    { // Top 3
        TileHandle top = getTopTileHandle(index);
        if (top.isValid()) {
            neighbors[(int)NeighborIndex8::TOP] = top.tile;
            TileHandle topLeft = top.chunk->getLeftTileHandle(top.index);
            neighbors[(int)NeighborIndex8::TOP_LEFT] = topLeft.tile;
            TileHandle topRight = top.chunk->getRightTileHandle(top.index);
            neighbors[(int)NeighborIndex8::TOP_RIGHT] = topRight.tile;
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
                    if (tile.groundLayer == TILE_ID_NONE) {
                        // If we have no ground layer, then we just set base Z to ground height
                        tile.setBaseZPosition(mWorldGrid->computeCenterHeightAtTile(mChunkId, tileIndex));
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
    Tile& oldTile = mTiles[i];
    TileFlags newFlags = TileFlags(oldTile.tileFlags | tile.tileFlags);
    oldTile = tile;
    oldTile.setTileFlags(newFlags); // Union tile flags
    // Update collision
    updateTileCollisionAt(i, tile.topLayer);
    dirtyNavGraph(); // TODO: Smarter?

    dirtyMesh();
}

void Chunk::setTileAt(TileIndex i, TileID tileId, TileLayer layer) {
    mTiles[i].layers[(int)layer] = tileId;
    // Update collision
    if (layer == TileLayer::Top) {
        // Onlu top tiles have colliders
        updateTileCollisionAt(i, tileId);
    }
    else if (layer == TileLayer::Ground) {
        dirtyNavGraph();
    }
    dirtyMesh();
}

void Chunk::setTileFlagAt(TileIndex i, TileFlags flag) {
    mTiles[i].setTileFlag(flag);
}

void Chunk::incRefNeighbors4() {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().incRef();
    getLeftNeighbor().incRef();
    getRightNeighbor().incRef();
    getTopNeighbor().incRef();
}

void Chunk::decRefNeighbors4() {
    assert(mDataReadyNeighborCount == 4);
    getBottomNeighbor().decRef();
    getLeftNeighbor().decRef();
    getRightNeighbor().decRef();
    getTopNeighbor().decRef();
}

void Chunk::updateTileCollisionAt(TileIndex i, TileID tileId) {
    Tile& tile = mTiles[i];
    tile.clearTileCollisionFlags();
    if (tileId == TILE_ID_NONE) {
        if (tile.tileFlags & TILE_FLAG_HAS_COLLIDER) {
            tile.tileFlags &= ~(TILE_FLAG_HAS_COLLIDER);
            dirtyNavGraph();
        }
    }
    else {
        const TileCollider& collider = TileRepository::getTileData(tileId).collider;
        if (collider.isValid()) {
            tile.tileFlags |= collider.defaultFlags;
            dirtyNavGraph();
        }
        else if (tile.tileFlags & TILE_FLAG_HAS_COLLIDER) {
            tile.tileFlags &= ~(TILE_FLAG_HAS_COLLIDER);
            dirtyNavGraph();
        }
    }
}
