#pragma once

typedef ui32 CollisionShapeID;
constexpr CollisionShapeID INVALID_COLLISION_SHAPE_ID = UINT32_MAX;

enum class CollisionShapes {
    NONE,
    CAPSULE,
    CYLINDER,
    BOX,
    SPHERE,
    MESH,
    TERRAIN,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(CollisionShapes,
    pair{ CollisionShapes::NONE, "none"sv },
    pair{ CollisionShapes::CAPSULE, "capsule"sv },
    pair{ CollisionShapes::CYLINDER, "cylinder"sv },
    pair{ CollisionShapes::BOX, "box"sv },
    pair{ CollisionShapes::SPHERE, "sphere"sv },
    pair{ CollisionShapes::MESH, "mesh"sv },
    pair{ CollisionShapes::TERRAIN, "terrain"sv }
);
static_assert(e_cast(CollisionShapes::COUNT) == 7, "Update def");