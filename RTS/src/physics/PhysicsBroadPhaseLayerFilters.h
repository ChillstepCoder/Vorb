#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/Body.h>

#include "physics/BroadphaseLayers.h"
#include "physics/PhysicsBodyUserData.h"

class PhysicsBroadphaseLayerFilterStatic : public JPH::BroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
        return inLayer == BroadPhaseLayers::Static;
    }
};

class PhysicsBroadphaseLayerFilterDynamic : public JPH::BroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
        return inLayer == BroadPhaseLayers::Dynamic;
    }
};

static_assert(BroadPhaseLayers::COUNT == 2, "Add new filters if needed");