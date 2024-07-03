#pragma once

#include "physics/PhysicsObjectLayer.h"

class PhysicsObjectLayerFilterStatic : public JPH::ObjectLayerFilter {
    public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == e_cast(PhysicsObjectLayer::Static);
    }
};

class PhysicsObjectLayerFilterDynamic : public JPH::ObjectLayerFilter {
    public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == e_cast(PhysicsObjectLayer::Dynamic);
    }
};