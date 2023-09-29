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
SERIALIZABLE_ENUM_SAME_NAME(CollisionShapes,
    pair{CollisionShapes::NONE, "none"sv },
    pair{ CollisionShapes::CAPSULE, "capsule"sv },
    pair{ CollisionShapes::CYLINDER, "cylinder"sv },
    pair{ CollisionShapes::BOX, "box"sv },
    pair{ CollisionShapes::SPHERE, "sphere"sv }
);
static_assert(e_cast(CollisionShapes::COUNT) == 4, "Update def");