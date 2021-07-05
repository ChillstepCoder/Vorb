#include "stdafx.h"
#include "Chunk.h"

#include "rendering/QuadMesh.h"

#include "world/WorldGrid.h"

#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"

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
}

void Chunk::allocateTiles() {
    // TODO: Not always
    mTiles.resize(CHUNK_SIZE);
}

void Chunk::freeTiles() {
    std::vector<Tile>().swap(mTiles);
}

void Chunk::dispose() {

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
    mChunkRenderData.mBaseDirty = true;
}

TileHandle Chunk::getLeftTile(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x > 0) {
        TileHandle handle(this, index - 1);
        handle.tile = mTiles[handle.index];
        return handle;
    }
    Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isDataReady()) {
        TileHandle handle(&leftNeighbor, index + CHUNK_WIDTH - 1);
        handle.tile = leftNeighbor.mTiles[handle.index];
        return handle;
    }
	return TileHandle();
}

TileHandle Chunk::getRightTile(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x < CHUNK_WIDTH - 1) {
        TileHandle handle(this, index + 1);
        handle.tile = mTiles[handle.index];
        return handle;
    }

    Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isDataReady()) {
        TileHandle handle(&rightNeighbor, index - CHUNK_WIDTH + 1);
        handle.tile = rightNeighbor.mTiles[handle.index];
        return handle;
    }
    return TileHandle();
}

TileHandle Chunk::getTopTile(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y < CHUNK_WIDTH - 1) {
        TileHandle handle(this, index + CHUNK_WIDTH);
        handle.tile = mTiles[handle.index];
        return handle;
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isDataReady()) {
        TileHandle handle(&topNeighbor, index + CHUNK_WIDTH - CHUNK_SIZE);
        handle.tile = topNeighbor.mTiles[handle.index];
        return handle;
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTile(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y > 0) {
        TileHandle handle(this, index - CHUNK_WIDTH);
        handle.tile = mTiles[handle.index];
        return handle;
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isDataReady()) {
        TileHandle handle(&bottomNeighbor, index - CHUNK_WIDTH + CHUNK_SIZE);
        handle.tile = bottomNeighbor.mTiles[handle.index];
        return handle;
    }
	return TileHandle();
}

void Chunk::getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTile(index);
		if (bottom.isValid()) {
			neighbors[(int)NeighborIndex::BOTTOM] = bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTile(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_LEFT] = bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTile(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_RIGHT] = bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex::LEFT] = getLeftTile(index).tile;

    // Right
    neighbors[(int)NeighborIndex::RIGHT] = getRightTile(index).tile;

    { // Top 3
        TileHandle top = getTopTile(index);
        if (top.isValid()) {
            neighbors[(int)NeighborIndex::TOP] = top.tile;
            TileHandle topLeft = top.chunk->getLeftTile(top.index);
            neighbors[(int)NeighborIndex::TOP_LEFT] = topLeft.tile;
            TileHandle topRight = top.chunk->getRightTile(top.index);
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

const ItemStack* Chunk::tryGetItemStackAt(TileIndex i) const {
    auto&& it = mItemsOnFloor.find(i);
    if (it == mItemsOnFloor.end()) {
        return nullptr;
    }
    return &it->second;
}

bool Chunk::tryAddFullItemStackAt(TileIndex i, ItemStack itemStack) {
    auto&& it = mItemsOnFloor.find(i);
    if (it != mItemsOnFloor.end()) {
        if (it->second.id != itemStack.id) {
            // Can't merge different item types
            return false;
        }
        // Check if we can fit our entire stack on existing stack
        const ui32 newTotal = it->second.quantity + itemStack.quantity;
        ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
        if (newTotal > itemRepo.getItem(itemStack.id).getStackSize()) {
            // Can't fit stack
            return false;
        }
        else {
            // Increase existing stack size
            it->second.quantity += itemStack.quantity;
            // Notify stockpile
            if (mTiles[i].tileFlags & TILE_FLAG_IS_STOCKPILE) {
                assert(false);
            }
            return true;
        }
    }
    mItemsOnFloor[i] = itemStack;
    // Notify stockpile
    if (mTiles[i].tileFlags & TILE_FLAG_IS_STOCKPILE) {
        assert(false);
    }
    return true;
}

ItemStack Chunk::tryAddPartialItemStackAt(TileIndex i, ItemStack itemStack) {
    auto&& it = mItemsOnFloor.find(i);
    if (it != mItemsOnFloor.end()) {
        if (it->second.id != itemStack.id) {
            // Can't merge different item types
            return itemStack;
        }
        // Check if we can fit our entire stack on existing stack
        const ui32 newTotal = it->second.quantity + itemStack.quantity;
        ui32 quantityToAdd = itemStack.quantity;

        ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
        const ui32 stackSize = itemRepo.getItem(itemStack.id).getStackSize();
        if (newTotal > stackSize) {
            // Can't fit full stack
            quantityToAdd = stackSize - it->second.quantity;
        }
        // Increase existing stack size
        it->second.quantity += quantityToAdd;
        itemStack.quantity -= quantityToAdd;
        // Notify stockpile
        if (mTiles[i].tileFlags & TILE_FLAG_IS_STOCKPILE) {
            assert(false);
        }
        return itemStack;
    }
    mItemsOnFloor[i] = itemStack;
    // Notify stockpile
    if (mTiles[i].tileFlags & TILE_FLAG_IS_STOCKPILE) {
        assert(false);
    }
    return itemStack;
}

ChunkID::ChunkID(const f32v2 worldPos) {
    assert(worldPos.x >= 0.0f && worldPos.y >= 0.0f);
	pos = i32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
    id = pos.y * WorldData::WORLD_WIDTH_CHUNKS + pos.x;
}

ChunkID::ChunkID(ui32 id) : 
    id(id) { 
    pos.x = id % WorldData::WORLD_WIDTH_CHUNKS;
    pos.y = id / WorldData::WORLD_WIDTH_CHUNKS;
};
