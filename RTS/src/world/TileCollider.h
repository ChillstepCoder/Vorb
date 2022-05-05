#pragma once

// TODO: Just index

#include "tile/TileFlags.h"

enum class TileCollisionShape : ui8 {
    NONE,
    BOX,
    CIRCLE,
    COUNT
};
//
//typedef ui16 TileColliderID;
//constexpr TileColliderID INVALID_COLLIDER_ID = UINT16_MAX;


constexpr ui16 TILE_COLLISION_FLAGS_MASK = TILE_FLAG_DOOR | TILE_FLAG_DOOR | TILE_FLAG_BREAKABLE;

struct TileCollider {
    f32v3 dims = f32v3(0.0f);
    TileFlags defaultFlags = TILE_FLAG_HAS_COLLIDER;
    TileCollisionShape shape = TileCollisionShape::NONE;

    const f32v2& getDimsXy() const { return reinterpret_cast<const f32v2&>(dims); } // It just works - Todd howard
    bool isValid() const { return shape != TileCollisionShape::NONE; }
};
static_assert(sizeof(TileCollider) == 16, "Keep it small as possible");
