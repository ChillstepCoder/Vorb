#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    FORCE_EXTERNAL_EDGE_SOUTH   = 1 << 0,
    FORCE_EXTERNAL_EDGE_WEST    = 1 << 1,
    FORCE_EXTERNAL_EDGE_EAST    = 1 << 2,
    FORCE_EXTERNAL_EDGE_NORTH   = 1 << 3,
    IS_INTERACTING              = 1 << 4,
    IS_STOCKPILE                = 1 << 5, // True if owned by a stockpile
     // True if inside city limits
    HAS_ITEM_STACK              = 1 << 6,
    IS_RESOURCE_RESERVED        = 1 << 7,
    IS_IMPASSABLE               = 1 << 8,
    IS_BLOCKED_BY_STRUCTURE     = 1 << 9,
    IN_CITY                     = 1 << 10,

    TERM                        = 1 << 10, // Keep this = last
};
static_assert(e_cast(TileFlags::TERM) <= 0x8000); // Must fit into a short

constexpr TileFlagType IMPASSABLE_TILE_FLAGS_MASK = e_cast(TileFlags::IS_IMPASSABLE) | e_cast(TileFlags::IS_BLOCKED_BY_STRUCTURE);