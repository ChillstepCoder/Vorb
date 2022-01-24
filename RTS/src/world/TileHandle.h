#pragma once

#include "ChunkID.h"
#include "Tile.h"

class Chunk;

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
    TileHandle(const Chunk* chunk, TileIndex index);

    bool isValid() const { return chunk != nullptr; }
    Chunk* getMutableChunk() { assert(IS_MAIN_THREAD());  return const_cast<Chunk*>(chunk); }
    f32v2 getWorldPos();

    TileHandle& operator=(const TileHandle& other) {
        chunk = other.chunk;
        const_cast<TileIndex&>(index) = other.index;
        tile = other.tile;
        return *this;
    }

    const Chunk* chunk = nullptr;
    const Tile* tile = nullptr;
    const TileIndex index;
};
static_assert(sizeof(TileHandle) == 24, "Keep small as possible");

// DOES NOT PROVIDE THREAD SAFE READ/WRITE
struct TileRef {
    TileRef();
    TileRef(TileHandle handle);
    TileRef(Chunk* chunk, TileIndex index);
    ~TileRef() { release(); }

    VORB_NON_COPYABLE(TileRef);
    TileRef(TileRef&& o) {
        // Moving calls destructor on other, so we should reaquire
        acquire(o.chunk, o.index);
    }
    TileRef& operator=(TileRef&& o) {
        // Moving calls destructor on other, so we should reaquire
        acquire(o.chunk, o.index);
    }

    void acquire(TileHandle handle);
    void acquire(Chunk* chunk, TileIndex index);
    void release();
    bool isValid() const { return chunk != nullptr; }

    Chunk* chunk = nullptr;
    Tile* tile = nullptr;
    TileIndex index;
};