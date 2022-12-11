#pragma once

enum class CollisionShapes {
    CAPSULE,
    CYLINDER,
    BOX,
    SPHERE,
    COUNT,
    NONE = COUNT
};
KEG_ENUM_DECL(CollisionShapes);