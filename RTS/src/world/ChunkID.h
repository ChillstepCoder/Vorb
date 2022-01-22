#pragma once

#include "world/WorldData.h"

constexpr ui32 CHUNK_ID_INVALID = UINT32_MAX;

// TODO: Shrink?
struct ChunkID {
    ChunkID() : id(CHUNK_ID_INVALID), pos(CHUNK_ID_INVALID) {}
    ChunkID(const ChunkID& other) { *this = other; }
    ChunkID(const ui32v2& pos) : pos(pos) { initIdFromPos(); };
    ChunkID(ui32v2&& pos) : pos(pos) { initIdFromPos(); };
    ChunkID(ui32 xPos, ui32 yPos) : pos(xPos, yPos) { initIdFromPos(); };
    ChunkID(const f32v2 worldPos) {
        assert(worldPos.x >= 0.0f && worldPos.y >= 0.0f);
        pos = ui32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
        id = pos.y * WorldData::WORLD_WIDTH_CHUNKS + pos.x;
    }

    static ChunkID fromWorldUI32v2(const ui32v2& worldPos) {
        ChunkID id;
        id.pos = ui32v2(worldPos.x / CHUNK_WIDTH, worldPos.y / CHUNK_WIDTH);
        id.id = id.pos.y * WorldData::WORLD_WIDTH_CHUNKS + id.pos.x;
        return id;
    }

    ChunkID(ui32 id) :
        id(id) {
        pos.x = id % WorldData::WORLD_WIDTH_CHUNKS;
        pos.y = id / WorldData::WORLD_WIDTH_CHUNKS;
    };

    // For std::map
    bool operator<(const ChunkID& other) const { return id < other.id; }
    bool operator!=(const ChunkID& other) const { return id != other.id; }
    bool operator==(const ChunkID& other) const { return id == other.id; }

    f32v2 getWorldPos() const { return f32v2(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH); }
    ChunkID getLeftID() const { return ChunkID(ui32v2(pos.x - 1, pos.y)); }
    ChunkID getTopID() const { return ChunkID(ui32v2(pos.x, pos.y + 1)); }
    ChunkID getRightID() const { return ChunkID(ui32v2(pos.x + 1, pos.y)); }
    ChunkID getBottomID() const { return ChunkID(ui32v2(pos.x, pos.y - 1)); }

    // Return true if we should never load
    bool isSentinelID() const {
        return pos.x == 0 || pos.y == 0 || pos.x == WorldData::WORLD_WIDTH_CHUNKS - 1 || pos.y == WorldData::WORLD_WIDTH_CHUNKS - 1;
    }

    bool isInvalid() const {
        return pos.x >= WorldData::WORLD_WIDTH_CHUNKS || pos.y >= WorldData::WORLD_WIDTH_CHUNKS;
    }

    ui32v2 pos;
    ui32 id;

private:
    inline void initIdFromPos() {
        id = pos.y * WorldData::WORLD_WIDTH_CHUNKS + pos.x;
    }
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