#pragma once

#include "world/TerrainConstants.h"

constexpr ui32 GRID_ID_INVALID = UINT32_MAX;
constexpr ui32 CHUNK_ID_INVALID = GRID_ID_INVALID;

template<ui32 GRIDWIDTH, i32 CELLWIDTH>
struct GridID {
    GridID() : id(GRID_ID_INVALID), pos(GRID_ID_INVALID) {}
    GridID(const GridID& other) { *this = other; }
    GridID(const ui32v2& pos) : pos(pos) { initIdFromPos(); };
    GridID(ui32v2&& pos) : pos(pos) { initIdFromPos(); };
    GridID(ui32 xPos, ui32 yPos) : pos(xPos, yPos) { initIdFromPos(); };
    GridID(const f32v2 worldPos) {
        assert(worldPos.x >= 0.0f && worldPos.y >= 0.0f);
        pos = ui32v2(floor(worldPos.x / CELLWIDTH), floor(worldPos.y / CELLWIDTH));
        id = pos.y * GRIDWIDTH + pos.x;
    }

    static GridID fromWorldUI32v2(const ui32v2& worldPos) {
        GridID id;
        id.pos = ui32v2(worldPos.x / CELLWIDTH, worldPos.y / CELLWIDTH);
        id.id = id.pos.y * GRIDWIDTH + id.pos.x;
        return id;
    }
    static GridID fromWorldUI16v2(const ui16v2& worldPos) {
        GridID id;
        id.pos = ui32v2(worldPos.x / CELLWIDTH, worldPos.y / CELLWIDTH);
        id.id = id.pos.y * GRIDWIDTH + id.pos.x;
        return id;
    }

    GridID(ui32 id) :
        id(id) {
        pos.x = id % GRIDWIDTH;
        pos.y = id / GRIDWIDTH;
    };

    // For std::map
    bool operator<(const GridID& other) const { return id < other.id; }
    bool operator!=(const GridID& other) const { return id != other.id; }
    bool operator==(const GridID& other) const { return id == other.id; }

    f32v2 getWorldPos() const { return f32v2(pos.x * CELLWIDTH, pos.y * CELLWIDTH); }
    GridID getLeftID() const { return GridID(ui32v2(pos.x - 1, pos.y)); }
    GridID getTopID() const { return GridID(ui32v2(pos.x, pos.y + 1)); }
    GridID getRightID() const { return GridID(ui32v2(pos.x + 1, pos.y)); }
    GridID getBottomID() const { return GridID(ui32v2(pos.x, pos.y - 1)); }

    // Return true if we should never load
    bool isSentinelID() const {
        return pos.x == 0 || pos.y == 0 || pos.x == GRIDWIDTH - 1 || pos.y == GRIDWIDTH - 1;
    }

    bool isInvalid() const {
        return pos.x >= GRIDWIDTH || pos.y >= GRIDWIDTH;
    }

    ui32v2 pos; // TODO: Compress pos to ui16v2 and union with ID. Take note of WorldData::WORLD_WIDTH_CHUNKS and make the X fit into as many bits exactly
    ui32 id;

protected:
    inline void initIdFromPos() {
        id = pos.y * GRIDWIDTH + pos.x;
    }
};

typedef GridID<WORLD_WIDTH_HEIGHTMAP_PATCHES, HEIGHTMAP_WIDTH> HeightmapPatchID;
typedef GridID<WorldData::WORLD_WIDTH_CHUNKS, CHUNK_WIDTH> ChunkID;

namespace {
    inline HeightmapPatchID heightmapPatchIDFromChunkID(ChunkID id) {
        return HeightmapPatchID(id.pos / HEIGHTMAP_PATCH_WIDTH_CHUNKS);
    }
}

// TODO: Somewhere else?
struct TilePosition {
    TilePosition() {};
    TilePosition(ChunkID chunkId, TileIndex tileIndex) : chunkId(chunkId), tileIndex(tileIndex)  {};

    ChunkID chunkId;
    TileIndex tileIndex;
};


// Hash function
namespace std {
    template <>
    struct hash<ChunkID>
    {
        size_t operator()(const ChunkID& id) const
        {
            // Compute individual hash values for two data members and combine them using XOR and bit shifting
            return hash<ui32>()(id.id);
        }
    };
}
