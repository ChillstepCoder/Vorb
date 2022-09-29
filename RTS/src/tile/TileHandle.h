#pragma once

#include "world/ChunkID.h"
#include "tile/Tile.h"

#include <boost/container_hash/hash.hpp>

class TileContainer;
struct LiteTileHandle;

struct TileHandle {

    TileHandle() {};
    TileHandle(const TileContainer* container, TileIndex tileIndex);

    bool isValid() const { return container != nullptr; }
    TileContainer* getMutableContainer() { assert(IS_MAIN_THREAD());  return const_cast<TileContainer*>(container); }
    i32v2 getWorldPos2D() const;
    i32v3 getWorldPos3D() const;
    ChunkID getChunkIDAtPos() const { return ChunkID::fromWorldI32v2(getWorldPos2D()); }
    ui32v3 getContainerOffset() const;
    LiteTileHandle toLiteTileHandle() const;

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

struct LiteTileHandle {
    LiteTileHandle() {};
    LiteTileHandle(TileContainerID containerId, TileIndex index) : containerId(containerId), index(index) {};

    TileContainer* getTileContainer() const;
    bool isValid() const { return  containerId != INVALID_TILE_CONTAINER_ID; }
    TileHandle toTileHandle() const;

    i32v3 getWorldPosition() const;

    bool operator==(const LiteTileHandle& rhs) const { return index == rhs.index && containerId == rhs.containerId; }
    bool operator!=(const LiteTileHandle& rhs) const { return index != rhs.index || containerId != rhs.containerId; }

    TileContainerID containerId = INVALID_TILE_CONTAINER_ID;
    TileIndex index = INVALID_TILE_INDEX;
};
static_assert(sizeof(LiteTileHandle) == 8, "Keep small as possible");

class LiteTileHandleHash {
public:
    size_t operator()(const LiteTileHandle& v) const {
        size_t h = 0;
        boost::hash_combine(h, v.containerId);
        boost::hash_combine(h, v.index);
        return h;
    }
};

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