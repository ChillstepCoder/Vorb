#pragma once

typedef ui32 CollisionShapeID;
constexpr CollisionShapeID INVALID_COLLISION_SHAPE_ID = UINT32_MAX;

enum class CollisionShapes {
    CAPSULE,
    CYLINDER,
    BOX,
    SPHERE,
    COUNT,
    NONE = COUNT
};
KEG_ENUM_DECL(CollisionShapes);