#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

// Each Body is in an ObjectLayer. If two object layers don't collide, the bodies inside those layers cannot collide. 
// You can define object layers in any way you like, it could be a simple number from 0 to N or it could be a bitmask. 
// Jolt supports 16 or 32 bit ObjectLayers through the JPH_OBJECT_LAYER_BITS define and you're free to define as many as 
// you like as they don't incur any overhead in the system.

enum class PhysicsObjectLayer : JPH::ObjectLayer {
    Static = BIT(0),
    Item = BIT(1),
    Character = BIT(2),
    COUNT = 3
};

constexpr JPH::ObjectLayer STATIC_OBJECT_LAYER_MASK = e_cast(PhysicsObjectLayer::Static);
constexpr JPH::ObjectLayer DYNAMIC_OBJECT_LAYER_MASK = ~STATIC_OBJECT_LAYER_MASK;

// Object Collision Masks
constexpr JPH::ObjectLayer ITEM_OBJECT_COLLISION_MASK = STATIC_OBJECT_LAYER_MASK | e_cast(PhysicsObjectLayer::Item);
constexpr JPH::ObjectLayer CHARACTER_OBJECT_COLLISION_MASK = STATIC_OBJECT_LAYER_MASK | e_cast(PhysicsObjectLayer::Character);

static_assert(e_count(PhysicsObjectLayer) <= 16, "Too many object layers defined, increase JPH_OBJECT_LAYER_BITS");