#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

// Each Body is in an ObjectLayer. If two object layers don't collide, the bodies inside those layers cannot collide. 
// You can define object layers in any way you like, it could be a simple number from 0 to N or it could be a bitmask. 
// Jolt supports 16 or 32 bit ObjectLayers through the JPH_OBJECT_LAYER_BITS define and you're free to define as many as 
// you like as they don't incur any overhead in the system.

// TODO: Bitmask for layers ex player, NPC, terrain, ect
enum class PhysicsObjectLayer : JPH::ObjectLayer {
    Static,
    Dynamic,
    COUNT
};
