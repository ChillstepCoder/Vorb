#pragma once

#include "physics/PhysicsObjectLayer.h"

class PhysicsObjectLayerFilterStatic : public JPH::ObjectLayerFilter {
    public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer & PhysicsObjectLayer::Static;
    }
};

class PhysicsObjectLayerFilterDynamic : public JPH::ObjectLayerFilter {
    public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer & (PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem);
    }
};