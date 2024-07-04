#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/Body.h>

#include "physics/PhysicsBodyUserData.h"

class PhysicsBodyFilterAttackable : public JPH::BodyFilter {
public:
    PhysicsBodyFilterAttackable() = default;
    PhysicsBodyFilterAttackable(PhysBodyID excludeId) : mExcludeId(excludeId) {}

    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        return inBodyID != mExcludeId;
    }

    bool ShouldCollideLocked(const JPH::Body& inBody) const override {
        PhysicsBodyUserDataType type = PhysicsBodyUserData(inBody.GetUserData()).getType();
        return type == PhysicsBodyUserDataType::Entity || type == PhysicsBodyUserDataType::Tile;
    }

    JPH::BodyID mExcludeId;
};

class PhysicsBodyFilterExcludeType : public JPH::BodyFilter {
public:
    PhysicsBodyFilterExcludeType(PhysicsBodyUserDataType type) : mType(type) {};
    PhysicsBodyFilterExcludeType(PhysicsBodyUserDataType type, PhysBodyID excludeId) : mType(type), mExcludeId(excludeId) {}

    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        return inBodyID != mExcludeId;
    }

    bool ShouldCollideLocked(const JPH::Body& inBody) const override {
        PhysicsBodyUserDataType type = PhysicsBodyUserData(inBody.GetUserData()).getType();
        return type != mType;
    }

    PhysicsBodyUserDataType mType;
    JPH::BodyID mExcludeId;
};

class PhysicsBodyFilterOnlyType : public JPH::BodyFilter {
public:
    PhysicsBodyFilterOnlyType(PhysicsBodyUserDataType type) : mType(type) {};
    PhysicsBodyFilterOnlyType(PhysicsBodyUserDataType type, PhysBodyID excludeId) : mType(type), mExcludeId(mExcludeId) {}

    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        return inBodyID != mExcludeId;
    }

    bool ShouldCollideLocked(const JPH::Body& inBody) const override {
        PhysicsBodyUserDataType type = PhysicsBodyUserData(inBody.GetUserData()).getType();
        return type == mType;
    }
    PhysicsBodyUserDataType mType;
    JPH::BodyID mExcludeId;
};

