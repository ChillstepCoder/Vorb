#pragma once

class Chunk;
class World;
class SimChunk;

struct ChunkLiteTileHandle {
    ChunkLiteTileHandle() {};
    ChunkLiteTileHandle(ChunkID chunkId, ChunkTileIndex index) : chunkId(chunkId), index(index) {};

    bool isValid() const { return  chunkId != INVALID_CHUNK_ID; }

    // Must be valid handle to call this
    i32v2 getWorldPosition2D(World& world) const;
    // Must be valid handle to call this
    Chunk& getChunk(World& world) const;
    // Must be valid handle to call this
    SimChunk& getSimChunk(World& world) const;

    void reset() { chunkId = INVALID_CHUNK_ID; }

    bool operator==(const ChunkLiteTileHandle& rhs) const { return index == rhs.index && chunkId == rhs.chunkId; }
    bool operator!=(const ChunkLiteTileHandle& rhs) const { return index != rhs.index || chunkId != rhs.chunkId; }

    ChunkID chunkId = INVALID_TILE_CONTAINER_ID;
    ChunkTileIndex index = INVALID_TILE_INDEX;
    TileID cachedTileID = TILE_ID_NONE; // External use only, included because we have 2 free bytes here
};

