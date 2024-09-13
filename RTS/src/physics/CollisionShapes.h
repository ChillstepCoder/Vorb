#pragma once

typedef ui32 CollisionShapeID;
constexpr CollisionShapeID INVALID_COLLISION_SHAPE_ID = UINT32_MAX;

enum class CollisionShapes {
    NONE,
    Capsule,
    Cylinder,
    Box,
    Sphere,
    Mesh,
    Terrain,
    COUNT
};
SERIALIZABLE_ENUM_DECL(CollisionShapes);