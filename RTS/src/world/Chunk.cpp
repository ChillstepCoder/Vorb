#include "stdafx.h"
#include "Chunk.h"

#include "rendering/QuadMesh.h"

#include "pathfinding/NavGraph.h"
#include "world/WorldGrid.h"

#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"

bool IS_SHUTTING_DOWN = false;

ChunkRenderData::~ChunkRenderData() {
    // Empty
    // TODO: RAII Wrapper for safety
    if (mLODTexture) {
        glDeleteTextures(1, &mLODTexture);
    }
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
    mCollision.resize(CHUNK_SIZE);
}

void Chunk::freeTiles() {
    std::vector<Tile>().swap(mTiles);
    std::vector<TileCollision>().swap(mCollision);
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
    mChunkRenderData.mLODDirty = true;
    mChunkRenderData.mMeshDirty = true;
    mChunkRenderData.mHighDetailFloraMeshDirty = true;
    mChunkRenderData.mIsVisible = false;

    if (mChunkRenderData.mLODTexture) {
        glDeleteTextures(1, &mChunkRenderData.mLODTexture);
        mChunkRenderData.mLODTexture = 0;
    }

    mChunkRenderData.mBillboardMesh.reset();
    mChunkRenderData.mChunkMesh.reset();
    mChunkRenderData.mHighDetailFloraMesh.reset();

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

const TileCollision& Chunk::getTileCollisionAt(const TileIndex index) const {
    return mCollision[index];
}

void Chunk::getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
			neighbors[(int)NeighborIndex::BOTTOM] = bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTileHandle(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_LEFT] = bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTileHandle(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_RIGHT] = bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex::LEFT] = getLeftTileHandle(index).tile;

    // Right
    neighbors[(int)NeighborIndex::RIGHT] = getRightTileHandle(index).tile;

    { // Top 3
        TileHandle top = getTopTileHandle(index);
        if (top.isValid()) {
            neighbors[(int)NeighborIndex::TOP] = top.tile;
            TileHandle topLeft = top.chunk->getLeftTileHandle(top.index);
            neighbors[(int)NeighborIndex::TOP_LEFT] = topLeft.tile;
            TileHandle topRight = top.chunk->getRightTileHandle(top.index);
            neighbors[(int)NeighborIndex::TOP_RIGHT] = topRight.tile;
        }
    }

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

void Chunk::setTileAt(TileIndex i, Tile tile) {
    assert(i < CHUNK_SIZE);
    Tile& oldTile = mTiles[i];
    TileFlags newFlags = TileFlags(oldTile.tileFlags | tile.tileFlags);
    oldTile = tile;
    oldTile.setTileFlags(newFlags); // Union tile flags
    // Update collision
    updateTileCollisionAt(i);

    dirtyMesh();
}

void Chunk::setTileAt(TileIndex i, TileID tileId, TileLayer layer) {
    mTiles[i].layers[(int)layer] = tileId;
    // Update collision
    if (layer == TileLayer::Top) {
        updateTileCollisionAt(i);
    }
    dirtyMesh();
}

void Chunk::setTileCollisionAt(TileIndex i, TileCollision collision) {
    TileCollisionNavFlags oldFlags = mCollision[i].flags;
    mCollision[i] = collision;
    mCollision[i].flags = TileCollisionNavFlags((ui8)collision.flags | (ui8)oldFlags);
}

void Chunk::setTileFlagAt(TileIndex i, TileFlags flag) {
    mTiles[i].setTileFlag(flag);
}

void Chunk::setTileCollisionNavFlagAt(TileIndex i, TileCollisionNavFlags flag) {
    mCollision[i].flags = TileCollisionNavFlags((ui8)mCollision[i].flags | (ui8)flag);
}

void Chunk::updateTileCollisionAt(TileIndex i) {

    // TODO: Multithreaded read, queued write
    //assert(!mIsNavmeshing);

    const Tile& tile = mTiles[i];
    TileCollision& collision = mCollision[i];
    TileCollisionNavFlags oldFlags = collision.flags;

    // Dirty navgraph when collision changes
    if (collision.baseZPosition != tile.baseZPosition) {
        dirtyNavGraph();
    }

    if (tile.topLayer != TILE_ID_NONE) {
        collision = tile.buildTileCollision();
    }
    else {
        collision = TileCollision();
        collision.baseZPosition = tile.baseZPosition;
    }
    collision.flags = TileCollisionNavFlags((ui8)collision.flags | (ui8)oldFlags);
}
