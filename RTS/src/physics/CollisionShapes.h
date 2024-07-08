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
SERIALIZABLE_ENUM_SAME_NAME(CollisionShapes,
    pair{ CollisionShapes::NONE, "none"sv },
    pair{ CollisionShapes::Capsule, "capsule"sv },
    pair{ CollisionShapes::Cylinder, "cylinder"sv },
    pair{ CollisionShapes::Box, "box"sv },
    pair{ CollisionShapes::Sphere, "sphere"sv },
    pair{ CollisionShapes::Mesh, "mesh"sv },
    pair{ CollisionShapes::Terrain, "terrain"sv }
);
static_assert(e_cast(CollisionShapes::COUNT) == 7, "Update def");