#pragma once

typedef ui16 TileID;
typedef ui32 TileContainerID;
typedef ui32 TileIndex;
typedef ui16 ChunkTileIndex;
typedef ui32 DTileIndex;
constexpr TileIndex INVALID_TILE_INDEX = std::numeric_limits<TileIndex>::max();;
constexpr TileContainerID INVALID_TILE_CONTAINER_ID = std::numeric_limits<TileContainerID>::max();;
constexpr ChunkTileIndex INVALID_CHUNK_TILE_INDEX = std::numeric_limits<ChunkTileIndex>::max();

constexpr TileID TILE_ID_NONE = std::numeric_limits<TileID>::max();
inline bool isTileNone(TileID tile) { return tile == TILE_ID_NONE; }
inline bool isTileValid(TileID tile) { return tile < TILE_ID_NONE; }

// For looking up tile by unique identifier in a tile container
// Does not store containerID so container must be known
struct TileKey {
    TileIndex tileIndex;
    TileID tileId;

    auto operator<=>(const TileKey&) const = default;
};
inline size_t hash_value(const TileKey& v) {
    size_t seed = 0;
    boost::hash_combine(seed, v.tileIndex);
    boost::hash_combine(seed, v.tileId);
    return seed;
}