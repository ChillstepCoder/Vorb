#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

enum class TileFlags : ui8 {
    TILE_FLAG_IS_INTERACTING = 1 << 0,
    TILE_FLAG_IS_STOCKPILE = 1 << 1, // True if owned by a stockpile
     // True if inside city limits
    TILE_FLAG_HAS_ITEM_STACK = 1 << 2,
    TILE_FLAG_DOOR = 1 << 3, // Lookup ownership for nav, can include windows for breakins?
    TILE_FLAG_HAS_COLLIDER = 1 << 4,
    TILE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 5,
    TILE_FLAG_IN_CITY = 1 << 6, // TODO: Remove?
    TILE_FLAG_IS_RESOURCE_RESERVED = 1 << 7,

    TILE_FLAG_TERM = 1 << 7, // Keep this at last
};
static_assert(e_cast(TileFlags::TILE_FLAG_TERM) <= 0x80); // Must fit into a byte