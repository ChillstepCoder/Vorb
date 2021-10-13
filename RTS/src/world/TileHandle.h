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
    Chunk* getMutableChunk() { return const_cast<Chunk*>(chunk); }
    f32v2 getWorldPos();

    TileHandle& operator=(const TileHandle& other) {
        chunk = other.chunk;
        const_cast<TileIndex&>(index) = other.index;
        const_cast<Tile&>(tile) = other.tile;
        return *this;
    }

    const Chunk* chunk = nullptr;
    const TileIndex index;
    const Tile tile;
};

// DOES NOT PROVIDE THREAD SAFE READ/WRITE
struct TileRef {
    TileRef();
    TileRef(TileHandle handle);
    TileRef(Chunk* chunk, TileIndex index);
    ~TileRef() { release(); }

    void acquire(TileHandle handle);
    void acquire(Chunk* chunk, TileIndex index);
    void release();
    bool isValid() const { return chunk != nullptr; }

    TileRef& operator=(const TileRef& other) = delete;

    Chunk* chunk = nullptr;
    TileIndex index;
    Tile* tile = nullptr;
};
