#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    IS_INTERACTING              = 1 << 0,
    IS_STOCKPILE                = 1 << 1, // True if owned by a stockpile
     // True if inside city limits
    HAS_ITEM_STACK              = 1 << 2,
    IS_RESOURCE_RESERVED        = 1 << 3,
    IS_BLOCKED_BY_STRUCTURE     = 1 << 4,
    IN_CITY                     = 1 << 5,
    BLOCKED_BY_SOUTH            = 1 << 6,
    BLOCKED_BY_WEST             = 1 << 7,
    BLOCKED_BY_EAST             = 1 << 8,
    BLOCKED_BY_NORTH            = 1 << 9,

    TERM                        = 1 << 9, // Keep this == last
};
static_assert(e_cast(TileFlags::TERM) <= 0x8000); // Must fit into a short

constexpr TileFlagType TILE_BLOCKED_TILE_FLAGS_MASK = e_cast(TileFlags::BLOCKED_BY_SOUTH) | e_cast(TileFlags::BLOCKED_BY_WEST) | e_cast(TileFlags::BLOCKED_BY_EAST) | e_cast(TileFlags::BLOCKED_BY_NORTH);
constexpr TileFlagType IMPASSABLE_TILE_FLAGS_MASK = TILE_BLOCKED_TILE_FLAGS_MASK | e_cast(TileFlags::IS_BLOCKED_BY_STRUCTURE);