#pragma once

#include <boost/container_hash/hash.hpp>

// Comment out for larger chunks
// #define USE_SMALL_CHUNK_WIDTH
constexpr int CHUNK_WIDTH = 128;
static_assert(CHUNK_WIDTH == 128, "Adjust bitwise operators below");
constexpr float CHUNK_DIAGONAL_RADIUS = 90.51f;
#define TILE_INDEX_CHUNK_MODULO_MASK 0x7f
#define TILE_INDEX_Y_SHIFT 7
#define TILE_INDEX_X_MASK TILE_INDEX_CHUNK_MODULO_MASK
// DoubleTile
// Grid used for roads, height, land ownership, and plots
constexpr int DTILE_WIDTH = 2;
// Grid used for biomes
constexpr int BLOCK_WIDTH = 8;

constexpr int CHUNK_WIDTH_DTILES = 128 / DTILE_WIDTH;
constexpr int CHUNK_SIZE_DTILES = SQ(CHUNK_WIDTH_DTILES);

constexpr int SUBCHUNK_WIDTH = 16;
constexpr int SUBCHUNK_WIDTH_SQ = SQ(SUBCHUNK_WIDTH);
constexpr int SUBCHUNKS_PER_CHUNK_ROW = CHUNK_WIDTH / SUBCHUNK_WIDTH;
constexpr int SUBCHUNKS_PER_CHUNK = SQ(SUBCHUNKS_PER_CHUNK_ROW);
/*
constexpr int MIN_SUBCHUNKS_PER_CHUNK_ROW = CHUNK_WIDTH / SUBCHUNK_WIDTH;
constexpr int MIN_SUBCHUNKS_PER_CHUNK = SQ(MIN_SUBCHUNKS_PER_CHUNK_ROW);*/

constexpr int HALF_CHUNK_WIDTH = CHUNK_WIDTH / 2;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_WIDTH;
constexpr int PADDED_CHUNK_WIDTH = CHUNK_WIDTH + 2;

template<typename Derived>
class CoordinateBase {
protected:
    explicit CoordinateBase(i32 xy) : v(xy) {}
    explicit CoordinateBase(i32 x, i32 y) : v(x, y) {}
    explicit CoordinateBase(i32v2 value) : v(value) {}
public:
    CoordinateBase() : v(0) {}

    Derived& operator+=(const Derived& other) {
        v += other.v;
        return *this;
    }
    Derived& operator-=(const Derived& other) {
        v -= other.v;
        return *this;
    }
    Derived& operator*=(const i32& other) {
        v *= other;
        return *this;
    }
    Derived& operator/=(const i32& other) {
        v /= other;
        return *this;
    }
    Derived operator+(const Derived& other) const { return Derived(v + other.v ); }
    Derived operator-(const Derived& other) const { return Derived(v - other.v ); }
    Derived operator*(const i32& other) const { return Derived(v * other ); }
    Derived operator/(const i32& other) const { return Derived(v / other ); }

    bool operator<(const CoordinateBase<Derived>& other) const {
        if (v.x < other.v.x) return true;
        if (v.x > other.v.x) return false;
        if (v.y < other.v.y) return true;
        return false;
    }
    bool operator==(const CoordinateBase<Derived>& other) const {
        return v.x == other.v.x && v.y == other.v.y;
    }

    ui32 toGridIDType(ui32 gridWidth) const { return v.y * gridWidth + v.x; }

    union {
        i32v2 v;
        struct {
            i32 x;
            i32 y;
        };
    };
};

template<typename T>
inline size_t hash_value(const CoordinateBase<T>& v) {
    size_t seed = 0;
    boost::hash_combine(seed, v.x);
    boost::hash_combine(seed, v.y);
    return seed;
};

class TileCoord;
class DTileCoord;
class BlockCoord;
class SubchunkCoord;
class ChunkCoord;

// Tile is the base unit of world, 1:1 with coordinate system
class TileCoord : public CoordinateBase<TileCoord> {
public:
    TileCoord() : CoordinateBase() {}
    explicit TileCoord(i32 xy) : CoordinateBase(xy) {}
    explicit TileCoord(i32 tileX, i32 tileY) : CoordinateBase(tileX, tileY) {}
    explicit TileCoord(i32v2 tilePos) : CoordinateBase(tilePos) {};
    explicit TileCoord(const DTileCoord& other);
    explicit TileCoord(const BlockCoord& other);
    explicit TileCoord(const SubchunkCoord& other);
    explicit TileCoord(const ChunkCoord& other);

    // Gets relative position within chunk
    TileIndex toChunkTileIndex() const {
        const i32v2 relativePos(v.x & TILE_INDEX_CHUNK_MODULO_MASK, v.y & TILE_INDEX_CHUNK_MODULO_MASK);
        return (relativePos.y << TILE_INDEX_Y_SHIFT) | relativePos.x;
    }
    inline ChunkID toChunkID(ui32 worldWidthChunks) const;
    inline std::pair<ChunkID, TileIndex> toChunkTileIndexAndChunkID(ui32 worldWidthChunks) const;
};

// 2x2 tiles, but offset by 1. I.e., 
// a DTileCoord offset from a chunk of value 0 will exist in 4 chunks simultaneously.
// This is convenient for things that map to terrain vertex painting, such as roads
// Used by Height, Road, Plot, Ownership
// Constructors by default take the round tile position rather than the floor position, corresponding to nearest height vertex
class DTileCoord : public CoordinateBase<DTileCoord> {
public:
    DTileCoord() : CoordinateBase() {}
    explicit DTileCoord(i32 xy) : CoordinateBase(xy) {}
    explicit DTileCoord(i32 x, i32 y) : CoordinateBase(x, y) {}
    explicit DTileCoord(i32v2 pos) : CoordinateBase(pos) {};
    explicit DTileCoord(const TileCoord& other);
    explicit DTileCoord(const BlockCoord& other);
    explicit DTileCoord(const SubchunkCoord& other);
    explicit DTileCoord(const ChunkCoord& other);

    void getCoveredTileCoords(OUT TileCoord coords[4]) {
        coords[3] = TileCoord(*this);
        coords[2] = coords[3] - TileCoord(1, 0);
        coords[1] = coords[3] - TileCoord(0, 1);
        coords[0] = coords[3] - TileCoord(1, 1);
    }

    i32v2 toTilePos() const { return v << 1; }
    // x,y,w,h
    i32v4 toTileAABBRound() const { return i32v4((v.x << 1) - 1, (v.y << 1) - 1, 2, 2); }
    static constexpr i32 getRowLengthPerChunk() { return CHUNK_WIDTH >> 1; }
    // Used for most things except structures, corresponds to height vertex
    static DTileCoord fromTilePosRound(i32v2 tilePos) { return DTileCoord(i32v2((tilePos.x + 1) >> 1, (tilePos.y + 1) >> 1)); }
    // Used for structures, corresponds to 2x2 tile blocks on a subgrid such as a structure/building
    static DTileCoord fromTilePosFloor(i32v2 tilePos) { return DTileCoord(i32v2(tilePos.x >> 1, tilePos.y >> 1)); }

    inline ChunkID toChunkID(ui32 worldWidthChunks) const;
};


inline size_t hash_value(const DTileCoord& c) {
    return glm::hash_value(c.v);
};

// 8x8 tiles
// Used by Biomes
class BlockCoord : public CoordinateBase<BlockCoord> {
public:
    BlockCoord() : CoordinateBase() {}
    explicit BlockCoord(i32 xy) : CoordinateBase(xy) {}
    explicit BlockCoord(i32 blockX, i32 blockY) : CoordinateBase(blockX, blockY) {}
    explicit BlockCoord(i32v2 blockPos) : CoordinateBase(blockPos) {}
    explicit BlockCoord(const TileCoord& other);
    explicit BlockCoord(const DTileCoord& other);
    explicit BlockCoord(const SubchunkCoord& other);
    explicit BlockCoord(const ChunkCoord& other);

    i32v2 toTilePos() const { return v << 3; }
    static constexpr i32 getRowLengthPerChunk() { return CHUNK_WIDTH >> 3; }
    static BlockCoord fromTilePos(i32v2 tilePos) { return BlockCoord(tilePos >> 3); }
};

// 16x16 tiles
// Used by coarse nav graph
class SubchunkCoord : public CoordinateBase<SubchunkCoord> {
public:
    SubchunkCoord() : CoordinateBase() {}
    explicit SubchunkCoord(i32 xy) : CoordinateBase(xy) {}
    explicit SubchunkCoord(i32 subchunkX, i32 subchunkY) : CoordinateBase(subchunkX, subchunkY) {}
    explicit SubchunkCoord(i32v2 subchunkPos) : CoordinateBase(subchunkPos) {}
    explicit SubchunkCoord(const TileCoord& other);
    explicit SubchunkCoord(const DTileCoord& other);
    explicit SubchunkCoord(const BlockCoord& other);
    explicit SubchunkCoord(const ChunkCoord& other);

    i32v2 toTilePos() const { return v << 4; }
    static SubchunkCoord fromTilePos(i32v2 tilePos) { return SubchunkCoord(tilePos >> 4); }
};

// 128x128 tiles
// Used by chunks
class ChunkCoord : public CoordinateBase<ChunkCoord> {
public:
    ChunkCoord() : CoordinateBase() {}
    explicit ChunkCoord(i32 xy) : CoordinateBase(xy) {}
    explicit ChunkCoord(i32 chunkX, i32 chunkY) : CoordinateBase(chunkX, chunkY) {}
    explicit ChunkCoord(i32v2 chunkPos) : CoordinateBase(chunkPos) {}
    explicit ChunkCoord(const TileCoord& other);
    explicit ChunkCoord(const DTileCoord& other);
    explicit ChunkCoord(const BlockCoord& other);
    explicit ChunkCoord(const SubchunkCoord& other);

    i32v2 toTilePos() const { return v << 7; }
    static ChunkCoord fromTilePos(i32v2 tilePos) { return ChunkCoord(tilePos >> 7); }
};
static_assert(sizeof(ChunkCoord) == sizeof(i32v2));

// Conversion implementations
// Inline functions defined after all class definitions to ensure visibility of all types

// TileCoord conversions
inline TileCoord::TileCoord(const DTileCoord& other) : CoordinateBase(other.v << 1) {}
inline TileCoord::TileCoord(const BlockCoord& other) : CoordinateBase(other.v << 3) {}
inline TileCoord::TileCoord(const SubchunkCoord& other) : CoordinateBase(other.v << 4) {}
inline TileCoord::TileCoord(const ChunkCoord& other) : CoordinateBase(other.v << 7) {}

// DTileCoord conversions
inline DTileCoord::DTileCoord(const TileCoord& other) : CoordinateBase(i32v2((other.x + 1) >> 1, (other.y + 1) >> 1)) {}
inline DTileCoord::DTileCoord(const BlockCoord& other) : CoordinateBase(other.v << 2) {}
inline DTileCoord::DTileCoord(const SubchunkCoord& other) : CoordinateBase(other.v << 3) {}
inline DTileCoord::DTileCoord(const ChunkCoord& other) : CoordinateBase(other.v << 6) {}

// BlockCoord conversions
inline BlockCoord::BlockCoord(const TileCoord& other) : CoordinateBase(other.v >> 3) {}
inline BlockCoord::BlockCoord(const DTileCoord& other) : CoordinateBase(other.v >> 2) {}
inline BlockCoord::BlockCoord(const SubchunkCoord& other) : CoordinateBase(other.v << 2) {}
inline BlockCoord::BlockCoord(const ChunkCoord& other) : CoordinateBase(other.v << 4) {}

// SubchunkCoord conversions
inline SubchunkCoord::SubchunkCoord(const TileCoord& other) : CoordinateBase(other.v >> 4) {}
inline SubchunkCoord::SubchunkCoord(const DTileCoord& other) : CoordinateBase(other.v >> 3) {}
inline SubchunkCoord::SubchunkCoord(const BlockCoord& other) : CoordinateBase(other.v >> 2) {}
inline SubchunkCoord::SubchunkCoord(const ChunkCoord& other) : CoordinateBase(other.v << 3) {}

// ChunkCoord conversions
inline ChunkCoord::ChunkCoord(const TileCoord& other) : CoordinateBase(other.v >> 7) {}
inline ChunkCoord::ChunkCoord(const DTileCoord& other) : CoordinateBase(other.v >> 6) {}
inline ChunkCoord::ChunkCoord(const BlockCoord& other) : CoordinateBase(other.v >> 4) {}
inline ChunkCoord::ChunkCoord(const SubchunkCoord& other) : CoordinateBase(other.v >> 3) {}

inline ChunkID TileCoord::toChunkID(ui32 worldWidthChunks) const {
    return ChunkCoord(*this).toGridIDType(worldWidthChunks);
}

inline std::pair<ChunkID, TileIndex> TileCoord::toChunkTileIndexAndChunkID(ui32 worldWidthChunks) const {
    return std::pair<TileIndex, ChunkID>(ChunkCoord(*this).toGridIDType(worldWidthChunks), toChunkTileIndex());
}

inline ChunkID DTileCoord::toChunkID(ui32 worldWidthChunks) const {
    return ChunkCoord(*this).toGridIDType(worldWidthChunks);
}

static_assert(CHUNK_WIDTH == 128);
static_assert(SUBCHUNK_WIDTH == 16);
static_assert(BLOCK_WIDTH == 8);
static_assert(DTILE_WIDTH == 2);