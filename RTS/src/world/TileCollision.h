#pragma once

enum class TileCollisionShape : ui8 {
    NONE,
    BOX,
    CIRCLE,
    COUNT
};

// TODO: Use
enum TileCollisionNavFlags : ui8 {
    COLLISION_NAV_FLAG_DOOR,      // Lookup ownership for nav
    COLLISION_NAV_FLAG_BREAKABLE, // Breakins :P
};

constexpr f32 TILE_COLLIDER_DIMS_SCALE = 254.0f;
constexpr i8 COLLIDER_DIMS_UNSCALED_MAX = 127;

// TODO: Flyweight?
struct TileCollision {
    TileCollisionNavFlags flags = (TileCollisionNavFlags)0u;
    TileCollisionShape shape = TileCollisionShape::NONE;
    ui8 baseZPosition = 0;
    ui8 roofZPosition = 0; // TODO: Use
    i8v2 colliderDimsUnscaledXY = i8v3(COLLIDER_DIMS_UNSCALED_MAX); // [-0.5f, 0.5f]
    ui16 colliderHeightUnscaled = UINT8_MAX;      // [0.0f, 256.0f] 

    f32v2 getColliderDimsScaledXY() const { return f32v2(colliderDimsUnscaledXY) / (f32)TILE_COLLIDER_DIMS_SCALE; }
    f32 getColliderHeightScaled() const { return (f32)colliderHeightUnscaled / (f32)UINT8_MAX; }
};
static_assert(sizeof(TileCollision) == 8, "Keep it small as possible");