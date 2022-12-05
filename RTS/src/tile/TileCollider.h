#pragma once

// TODO: Just index

#include "tile/TileFlags.h"

enum class TileCollisionShape : ui8 {
    NONE,
    BOX,
    CYLINDER,
    COUNT
};
KEG_ENUM_DECL(TileCollisionShape);
//
//typedef ui16 TileColliderID;
//constexpr TileColliderID INVALID_COLLIDER_ID = UINT16_MAX;

struct TileCollider {
    f32v3 dims = f32v3(0.0f);
    TileFlags defaultFlags = (TileFlags)0;
    TileCollisionShape shape = TileCollisionShape::NONE;

    const f32v2& getDimsXy() const { return reinterpret_cast<const f32v2&>(dims); } // It just works - Todd howard
    bool isValid() const { return shape != TileCollisionShape::NONE; }
};
static_assert(sizeof(TileCollider) == 16, "Keep it small as possible");
