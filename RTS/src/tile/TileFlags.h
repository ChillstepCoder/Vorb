#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    // First N bits must be "blocked/blocker" bits
    HAS_SOUTH_BLOCKER           = BIT(0),
    HAS_WEST_BLOCKER            = BIT(1),
    HAS_EAST_BLOCKER            = BIT(2),
    HAS_NORTH_BLOCKER           = BIT(3), // We require two of the above 4 bits to be set to count as a block, unless a larger block bit is set
    BLOCKED_BY_LARGE            = BIT(4),
    MEDIUM_BLOCKER              = BIT(5),
    LARGE_BLOCKER               = BIT(6),
    IS_BLOCKED_BY_STRUCTURE     = BIT(7),
    NAV_BLOCKED_MASK_TERM       = IS_BLOCKED_BY_STRUCTURE,
    // Bits beyond here do not contribute to nav blocking
    IS_INTERACTING              = BIT(8),
    IS_STOCKPILE                = BIT(9), // True if owned by a stockpile
    HAS_ITEM_STACK              = BIT(10),
    IS_RESOURCE_RESERVED        = BIT(11),
    IN_CITY                     = BIT(12), // True if inside city limits

    TERM                        = IN_CITY, // Keep this == last
};
static_assert(e_cast(TileFlags::TERM) <= 0x8000); // Must fit into a short

// These first N bits contribute to what is considered a "blocked tile"
// If two of the first 4 bits are set, we are always blocked, probably by a "medium" navmesh blocker
// like a tree. If we are blocked by a large, or ourselves are a large blocker, or structure,
// one of bits >= 4 will be set which is always considered blocked
constexpr TileFlagType TILE_BLOCKED_TILE_FLAGS_MASK = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7);
static_assert(e_cast(TileFlags::NAV_BLOCKED_MASK_TERM) == BIT(7));
static_assert(TileFlags::NAV_BLOCKED_MASK_TERM == TileFlags::IS_BLOCKED_BY_STRUCTURE);
constexpr bool IsTileNavBlocked(TileFlagType flags) {

    // We want these to be first 4 bits for table lookup without bitshift
    static_assert(e_cast(TileFlags::HAS_NORTH_BLOCKER) == BIT(3));
    // True if 2 or more bits are set
    constexpr bool BLOCKED_LOOKUP[16] = {
        false, // 0000
        false, // 0001
        false, // 0010
        true,  // 0011
        false, // 0100
        true,  // 0101
        true,  // 0110
        true,  // 0111
        false, // 1000
        true,  // 1001
        true,  // 1010
        true,  // 1011
        true,  // 1100
        true,  // 1101
        true,  // 1110
        true,  // 1111
    };

    const TileFlagType masked = flags & TILE_BLOCKED_TILE_FLAGS_MASK;
    return (masked > e_cast(TileFlags::HAS_NORTH_BLOCKER)) || BLOCKED_LOOKUP[masked & 0b1111];
}