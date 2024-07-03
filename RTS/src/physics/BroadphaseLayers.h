#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Broadphase/BroadPhaseLayer.h>

// We should use as few broadphase layers as possible to keep the broadphase efficient.
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer Static(0);
    static constexpr JPH::BroadPhaseLayer Dynamic(1);
    static constexpr ui32 COUNT(2);
};

