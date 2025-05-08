#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

// Each Body is in an ObjectLayer. If two object layers don't collide, the bodies inside those layers cannot collide. 
// You can define object layers in any way you like, it could be a simple number from 0 to N or it could be a bitmask. 
// Jolt supports 16 or 32 bit ObjectLayers through the JPH_OBJECT_LAYER_BITS define and you're free to define as many as 
// you like as they don't incur any overhead in the system.

namespace PhysicsObjectLayer {
    constexpr JPH::ObjectLayer Static = BIT(0);
    constexpr JPH::ObjectLayer DynamicSolid = BIT(1);
    constexpr JPH::ObjectLayer DynamicItem = BIT(2);
    constexpr JPH::ObjectLayer Character = BIT(3);
    constexpr JPH::ObjectLayer QueryPhantom = BIT(4);
    //constexpr JPH::ObjectLayer Interactable = BIT(5); // Maybe?
    constexpr JPH::ObjectLayer COUNT = 5;
};


static_assert(PhysicsObjectLayer::COUNT <= 8, "Too many object layer bits defined, increase JPH_OBJECT_LAYER_BITS");