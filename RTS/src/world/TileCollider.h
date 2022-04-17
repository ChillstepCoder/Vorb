#pragma once

// TODO: Just index

enum class TileCollisionShape : ui8 {
    NONE,
    BOX,
    CIRCLE,
    COUNT
};
//
//typedef ui16 TileColliderID;
//constexpr TileColliderID INVALID_COLLIDER_ID = UINT16_MAX;

// TODO: Move to new file
enum TileFlags : ui16 {
    TILE_FLAG_IS_INTERACTING       = 1 << 0,
    TILE_FLAG_IS_STOCKPILE         = 1 << 1, // True if owned by a stockpile
    TILE_FLAG_IN_CITY              = 1 << 2, // True if inside city limits
    TILE_FLAG_HAS_ITEM_STACK       = 1 << 3,
    TILE_FLAG_IS_BUILDING          = 1 << 4,
    TILE_FLAG_IS_LARGE_OBJECT_ROOT = 1 << 5, // Render root for large objects
    TILE_FLAG_DOOR                 = 1 << 6, // Lookup ownership for nav
    TILE_FLAG_BREAKABLE            = 1 << 7, // Breakins :P
    TILE_FLAG_ROAD                 = 1 << 8,
    TILE_FLAG_HAS_ROOF             = 1 << 9,
    TILE_FLAG_HAS_COLLIDER         = 1 << 10,
    TILE_FLAG_QUEUED_UPDATE        = 1 << 11,
    TILE_FLAG_IS_RESOURCE_RESERVED = 1 << 12,
    TILE_FLAG_IS_MULTI_FLOOR       = 1 << 13,
    //TILE_FLAG_IS_WORK_RESERVED     = 1 << 13,

    TILE_FLAG_TERM                 = 1 << 14, // Keep this at +1
};
static_assert(TILE_FLAG_TERM <= 0x8000); // Must fit into a short

constexpr ui16 TILE_COLLISION_FLAGS_MASK = TILE_FLAG_DOOR | TILE_FLAG_DOOR | TILE_FLAG_BREAKABLE;

struct TileCollider {
    f32v3 dims = f32v3(0.0f);
    TileFlags defaultFlags = TILE_FLAG_HAS_COLLIDER;
    TileCollisionShape shape = TileCollisionShape::NONE;

    const f32v2& getDimsXy() const { return reinterpret_cast<const f32v2&>(dims); } // It just works - Todd howard
    bool isValid() const { return shape != TileCollisionShape::NONE; }
};
static_assert(sizeof(TileCollider) == 16, "Keep it small as possible");
