#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    BLOCKED_BY_SOUTH            = BIT(0), // START HERE
    BLOCKED_BY_WEST             = BIT(1),
    BLOCKED_BY_EAST             = BIT(2),
    BLOCKED_BY_NORTH            = BIT(3), // We require two of the above 4 bits to be set to count as a block, unless a larger block bit is set
    BLOCKED_NAV_MIN             = BLOCKED_BY_NORTH, // If flags & BLOCKED_FLAGS_MASK is greater than this, we are nav blocked
    BLOCKED_BY_LARGE            = BIT(4),
    MEDIUM_BLOCKER              = BIT(5),
    LARGE_BLOCKER               = BIT(6),
    IS_BLOCKED_BY_STRUCTURE     = BIT(7),
    NAV_BLOCKED_MASK_TERM       = IS_BLOCKED_BY_STRUCTURE,
    IS_INTERACTING              = BIT(8),
    IS_STOCKPILE                = BIT(9), // True if owned by a stockpile
    HAS_ITEM_STACK              = BIT(10),
    IS_RESOURCE_RESERVED        = BIT(11),
    IN_CITY                     = BIT(12), // True if inside city limits

    TERM                        = IN_CITY, // Keep this == last
};
static_assert(e_cast(TileFlags::TERM) <= 0x8000); // Must fit into a short

constexpr TileFlagType TILE_BLOCKED_TILE_FLAGS_MASK = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7);
static_assert(TileFlags::NAV_BLOCKED_MASK_TERM == TileFlags::IS_BLOCKED_BY_STRUCTURE);
constexpr bool IsTileNavBlocked(TileFlags flags) {
    xxx; // WRONG! 0011 is less but still blocked
    return (e_cast(flags) & TILE_BLOCKED_TILE_FLAGS_MASK) > e_cast(TileFlags::BLOCKED_NAV_MIN);
}