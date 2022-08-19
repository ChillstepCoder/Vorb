#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    TILE_FLAG_FORCE_EXTERNAL_EDGE_SOUTH   = 1 << 0,
    TILE_FLAG_FORCE_EXTERNAL_EDGE_WEST    = 1 << 1,
    TILE_FLAG_FORCE_EXTERNAL_EDGE_EAST    = 1 << 2,
    TILE_FLAG_FORCE_EXTERNAL_EDGE_NORTH   = 1 << 3,
    TILE_FLAG_IS_INTERACTING              = 1 << 4,
    TILE_FLAG_IS_STOCKPILE                = 1 << 5, // True if owned by a stockpile
     // True if inside city limits
    TILE_FLAG_HAS_ITEM_STACK              = 1 << 6,
    TILE_FLAG_QUEUED_THREADSAFE_UPDATE    = 1 << 7,
    TILE_FLAG_IS_RESOURCE_RESERVED        = 1 << 8,
    TILE_FLAG_IS_IMPASSABLE               = 1 << 9,
    TILE_FLAG_IS_BLOCKED_BY_STRUCTURE     = 1 << 10,
    TILE_FLAG_IN_CITY                     = 1 << 11,

    TILE_FLAG_TERM                        = 1 << 11, // Keep this = last
};
static_assert(e_cast(TileFlags::TILE_FLAG_TERM) <= 0x8000); // Must fit into a short

constexpr TileFlagType IMPASSABLE_TILE_FLAGS_MASK = e_cast(TileFlags::TILE_FLAG_IS_IMPASSABLE) | e_cast(TileFlags::TILE_FLAG_IS_BLOCKED_BY_STRUCTURE);