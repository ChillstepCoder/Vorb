#pragma once

#include "tile/Tile.h"

#include <boost/container_hash/hash.hpp>

class World;
class TileContainer;
struct LiteTileHandle;
class SimChunk;
class LocalChunk;

struct TileHandle {

    TileHandle() {};
    TileHandle(const TileContainer* container, TileIndex tileIndex);

    bool isValid() const { return container != nullptr; }
    TileContainer* getMutableContainer() { ASSERT_GAME_THREAD();  return const_cast<TileContainer*>(container); }
    i32v2 getWorldPos2D() const;
    i32v3 getWorldPos3D() const;
    ui32v3 getContainerOffset() const;
    const Tile& getTile() const;
    const TileID getFloorTile() const;
    LiteTileHandle toLiteTileHandle() const;
    ChunkID getChunkIDAtPos() const;
    World& getWorld() const;

    void reset() { container = nullptr; }

    TileHandle& operator=(const TileHandle& other) {
        container = other.container;
        const_cast<TileIndex&>(tileIndex) = other.tileIndex;
        return *this;
    }

    const TileContainer* container = nullptr;
    const TileIndex tileIndex = INVALID_TILE_INDEX;
};
static_assert(sizeof(TileHandle) == 16, "Keep small as possible");

struct LiteTileHandle {
    LiteTileHandle() {};
    LiteTileHandle(TileContainerID containerId, TileIndex index) : containerId(containerId), index(index) {};
    LiteTileHandle(std::pair<TileContainerID, TileIndex> pair) : containerId(pair.first), index(pair.second) {};

    TileContainer* getTileContainer(World& world) const;
    TileContainer* tryGetTileContainer(World& world) const;
    bool isValid() const { return  containerId != INVALID_TILE_CONTAINER_ID; }
    void invalidate() { containerId = INVALID_TILE_CONTAINER_ID; }
    TileHandle toTileHandle(World& world) const;

    i32v3 getWorldPosition(World& world) const;
    f32v3 getWorldPositionCenter(World& world) const;

    void reset() { containerId = INVALID_TILE_CONTAINER_ID; }

    bool operator==(const LiteTileHandle& rhs) const { return index == rhs.index && containerId == rhs.containerId; }
    bool operator!=(const LiteTileHandle& rhs) const { return index != rhs.index || containerId != rhs.containerId; }

    TileContainerID containerId = INVALID_TILE_CONTAINER_ID;
    TileIndex index = INVALID_TILE_INDEX;
};
static_assert(sizeof(LiteTileHandle) == 8, "Keep small as possible");

template<>
struct std::less<LiteTileHandle>
{
    bool operator() (const LiteTileHandle& a, const LiteTileHandle& b) const
    {
        if (a.containerId < b.containerId) return true;
        if (a.containerId > b.containerId) return false;
        return a.index < b.index;
    }
};

inline size_t hash_value(const LiteTileHandle& v) {
    size_t h = 0;
    boost::hash_combine(h, v.containerId);
    boost::hash_combine(h, v.index);
    return h;
}

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

    i32v2 getWorldPos2D() const;
    i32v3 getWorldPos3D() const;

    TileContainer* container = nullptr;
    Tile* tile = nullptr;
    TileIndex index;
};