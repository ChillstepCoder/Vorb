#pragma once

#include "physics/CollisionShapes.h"

struct PhysicsShapeUserData {
    PhysicsShapeUserData() = default;
    PhysicsShapeUserData(ui64 data) : data(data) {}
    PhysicsShapeUserData(CollisionShapes shape) : data(ui64(shape) | ((ui64)INVALID_COLLISION_SHAPE_ID << 32)) {}
    PhysicsShapeUserData(CollisionShapeID id, CollisionShapes shape) : data(ui64(shape) | ((ui64)id << 32)) {}

    CollisionShapes getShapeType() const { return static_cast<CollisionShapes>(data & 0xFFFFFFFF); }
    CollisionShapeID getShapeID() const { return static_cast<CollisionShapeID>(data >> 32); }
    operator ui64() const { return std::bit_cast<ui64>(*this); }

    ui64 data = 0;
};