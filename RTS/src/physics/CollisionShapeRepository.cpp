#include "stdafx.h"
#include "CollisionShapeRepository.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "physics/PhysicsShapeUserData.h"

CollisionShapeRepository* sCollisionShapeRepository = nullptr;

CollisionShapeRepository::CollisionShapeRepository() {
    assert(sCollisionShapeRepository == nullptr);
    sCollisionShapeRepository = this;
}

CollisionShapeRepository::~CollisionShapeRepository() {
    assert(sCollisionShapeRepository == this);
    sCollisionShapeRepository = nullptr;
}

CollisionShapeRepository& CollisionShapeRepository::get() {
    assert(sCollisionShapeRepository);
    return *sCollisionShapeRepository;
}

CollisionShapeID CollisionShapeRepository::getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
    switch (shapeType) {
        case CollisionShapes::Capsule:
            return getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.y);
        case CollisionShapes::Cylinder:
            return getOrAddCylinderCollisionShape(halfExtents);
        case CollisionShapes::Box:
            return getOrAddBoxCollisionShape(halfExtents);
        case CollisionShapes::Sphere:
            return getOrAddSphereCollisionShape(halfExtents.x);
        default:
            panic("Invalid shape type {} in getOrAddCollisionShape", (int)shapeType);
            break;

    }
    static_assert(e_cast(CollisionShapes::COUNT) == 7, "Handle new shapes");
    return INVALID_COLLISION_SHAPE_ID;
}

CollisionShapeID CollisionShapeRepository::getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
    // Find existing
    const f32v2 key(radius, halfHeight);
    auto&& it = mCapsuleShapes.find(key);
    if (it != mCapsuleShapes.end()) {
        return it->second;
    }

    // Insert new
    const CollisionShapeID id = mAllJoltShapes.size();
    JPH::CapsuleShapeSettings settings(halfHeight, radius);
    JPH::ShapeSettings::ShapeResult res = settings.Create();
    if (res.HasError()) {
        panic("Failed to create capsule shape: {}", res.GetError().c_str());
    }
    if (settings.IsSphere()) {
        res.Get()->SetUserData(PhysicsShapeUserData(CollisionShapes::Sphere));
    }
    else {
        res.Get()->SetUserData(PhysicsShapeUserData(CollisionShapes::Capsule));
    }
    mAllJoltShapes.emplace_back(res.Get());
    mCapsuleShapes[key] = id;
    return id;
}

CollisionShapeID CollisionShapeRepository::getOrAddCylinderCollisionShape(const f32v3& halfExtents) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
    // Find existing
    auto&& it = mCylinderShapes.find(halfExtents);
    if (it != mCylinderShapes.end()) {
        return it->second;
    }

    // Insert new
    const CollisionShapeID id = mAllJoltShapes.size();
    JPH::CylinderShapeSettings settings(halfExtents.y, halfExtents.x);
    JPH::ShapeSettings::ShapeResult res = settings.Create();
    if (res.HasError()) {
        panic("Failed to create cylinder shape: {}", res.GetError().c_str());
    }
    res.Get()->SetUserData(PhysicsShapeUserData(id, CollisionShapes::Cylinder));
    mAllJoltShapes.emplace_back(res.Get());
    mCylinderShapes[halfExtents] = id;
    return id;
}

CollisionShapeID CollisionShapeRepository::getOrAddBoxCollisionShape(const f32v3& halfExtents) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
    // Find existing
    auto&& it = mBoxShapes.find(halfExtents);
    if (it != mBoxShapes.end()) {
        return it->second;
    }

    // Insert new
    const CollisionShapeID id = mAllJoltShapes.size();
    JPH::BoxShapeSettings data(JPH::Vec3Arg(halfExtents.x, halfExtents.y, halfExtents.z));
    JPH::ShapeSettings::ShapeResult res = data.Create();
    if (res.HasError()) {
        panic("Failed to create box shape: {}", res.GetError().c_str());
    }
    res.Get()->SetUserData(PhysicsShapeUserData(id, CollisionShapes::Box));
    mAllJoltShapes.emplace_back(res.Get());
    mBoxShapes[halfExtents] = id;
    return id;
}

CollisionShapeID CollisionShapeRepository::getOrAddSphereCollisionShape(f32 radius) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
    // Find existing
    auto&& it = mSphereShapes.find(radius);
    if (it != mSphereShapes.end()) {
        return it->second;
    }

    // Insert new
    const CollisionShapeID id = mAllJoltShapes.size();
    JPH::SphereShapeSettings settings(radius);
    JPH::ShapeSettings::ShapeResult res = settings.Create();
    if (res.HasError()) {
        panic("Failed to create box shape: {}", res.GetError().c_str());
    }
    res.Get()->SetUserData(PhysicsShapeUserData(id, CollisionShapes::Sphere));
    mAllJoltShapes.emplace_back(res.Get());
    mSphereShapes[radius] = id;
    return id;
}

CollisionShapeID CollisionShapeRepository::addCompoundCollisionShape(std::span<const ModelColliderShape> shapes) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup

    // Insert new
    const CollisionShapeID id = mAllJoltShapes.size();
    mAllJoltShapes.emplace_back();
    JPH::StaticCompoundShapeSettings settings;
    for (const ModelColliderShape& shape : shapes) {
        settings.AddShape(
            JPH::Vec3(shape.mOffset.x, shape.mOffset.y, shape.mOffset.z),
            JPH::Quat::sEulerAngles(JPH::Vec3(shape.mEulerAngles.x, shape.mEulerAngles.y, shape.mEulerAngles.z)),
            mAllJoltShapes[getOrAddCollisionShape(shape.mShape, shape.mHalfDims)],
            PhysicsShapeUserData(id, shape.mShape)
        );
    }
    mAllJoltShapes[id] = settings.Create().Get();
    return id;
}
