#pragma once

enum class TileBaseFlags : ui8 {
    TILE_BASE_FLAG_QUEUED_THREADSAFE_UPDATE = 1 << 2,
};

typedef ui16 TileFlagType;
enum class TileFlags : TileFlagType {
    // First N bits must be "blocked/blocker" bits
    IS_BLOCKED_BY_BUILDING      = BIT(0),
    // Bits beyond here do not contribute to nav blocking
    IS_INTERACTING              = BIT(1),
    IS_DAMAGED                  = BIT(2),
    HAS_ITEM_STACK              = BIT(3),
    IS_RESOURCE_RESERVED        = BIT(4),
    IN_CITY                     = BIT(5), // True if inside city limits
    ROOFED                      = BIT(6), // True if inside city limits
    IS_BUILDING_EXTERIOR        = BIT(7),
    //IS_HARVESTABLE              = BIT(8),

    E, // End marker for calculating TERM
    TERM                        = E - 1,
};
static_assert(e_cast(TileFlags::TERM) >= BIT(3), "If small, we overflowed the TileFlagType");


constexpr bool IsTileNavBlocked(TileFlagType flags) {
    return (flags & e_cast(TileFlags::IS_BLOCKED_BY_BUILDING)) != 0;
}