#pragma once

#include "world/ChunkID.h"
#include "tile/Tile.h"

class TileContainer;

struct LiteTileHandle {
    LiteTileHandle() {};
    LiteTileHandle(ChunkID chunkID, TileIndex index) : chunkID(chunkID), index(index) {};

    f32v2 getWorldPos() const {
        f32v2 rv = chunkID.getWorldPos();
        rv.x += index.getX();
        rv.y += index.getY();
        return rv;
    }

    ChunkID chunkID;
    TileIndex index;
};

struct TileHandle {

    TileHandle() {};
    TileHandle(const TileContainer* container, TileIndex index);

    bool isValid() const { return container != nullptr; }
    TileContainer* getMutableContainer() { assert(IS_MAIN_THREAD());  return const_cast<TileContainer*>(container); }
    ui32v2 getWorldPos2D() const;
    ChunkID getChunkIDAtPos() const { return ChunkID::fromWorldUI32v2(getWorldPos2D()); }

    TileHandle& operator=(const TileHandle& other) {
        container = other.container;
        const_cast<TileIndex&>(index) = other.index;
        tile = other.tile;
        return *this;
    }

    const TileContainer* container = nullptr;
    const Tile* tile = nullptr;
    const TileIndex index;
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