#pragma once

#include "world/ChunkID.h"
#include "tile/Tile.h"

class TileContainer;

struct TileHandle {

    TileHandle() {};
    TileHandle(const TileContainer* container, TileIndex tileIndex);

    bool isValid() const { return container != nullptr; }
    TileContainer* getMutableContainer() { assert(IS_MAIN_THREAD());  return const_cast<TileContainer*>(container); }
    ui32v2 getWorldPos2D() const;
    ChunkID getChunkIDAtPos() const { return ChunkID::fromWorldUI32v2(getWorldPos2D()); }
    ui32v3 getContainerOffset() const;

    TileHandle& operator=(const TileHandle& other) {
        container = other.container;
        const_cast<TileIndex&>(tileIndex) = other.tileIndex;
        tile = other.tile;
        return *this;
    }

    const TileContainer* container = nullptr;
    const Tile* tile = nullptr;
    const TileIndex tileIndex = INVALID_TILE_INDEX;
};
static_assert(sizeof(TileHandle) == 24, "Keep small as possible");

// DOES NOT PROVIDE THREAD SAFE READ/WRITE
struct TileRef {
    TileRef();
    TileRef(TileHandle handle);
    TileRef(TileContainer* container, TileIndex index);
    ~TileRef() { release(); }

    VORB_NON_COPYABLE(TileRef);
    TileRef(TileRef&& o) {
        // Moving calls destructor on other, so we should reaquire
        acquire(o.container, o.index);
    }
    TileRef& operator=(TileRef&& o) {
        // Moving calls destructor on other, so we should reaquire
        acquire(o.container, o.index);
    }

    void acquire(TileHandle handle);
    void acquire(TileContainer* container, TileIndex index);
    void release();
    bool isValid() const { return container != nullptr; }

    TileContainer* container = nullptr;
    Tile* tile = nullptr;
    TileIndex index;
};