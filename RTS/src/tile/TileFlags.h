#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

enum class TileFlags : ui8 {
    TILE_FLAG_IS_INTERACTING = 1 << 0,
    TILE_FLAG_IS_STOCKPILE = 1 << 1, // True if owned by a stockpile
     // True if inside city limits
    TILE_FLAG_HAS_ITEM_STACK = 1 << 2,
    TILE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 3,
    TILE_FLAG_IS_RESOURCE_RESERVED = 1 << 4,
    TILE_FLAG_IS_IMPASSABLE = 1 << 5,
    TILE_FLAG_IS_BLOCKED_BY_STRUCTURE = 1 << 6,
    TILE_FLAG_IN_CITY = 1 << 7,

    TILE_FLAG_TERM = 1 << 7, // Keep this = last
};
static_assert(e_cast(TileFlags::TILE_FLAG_TERM) <= 0x80); // Must fit into a byte

constexpr ui8 IMPASSABLE_TILE_FLAGS_MASK = e_cast(TileFlags::TILE_FLAG_IS_IMPASSABLE) | e_cast(TileFlags::TILE_FLAG_IS_BLOCKED_BY_STRUCTURE);