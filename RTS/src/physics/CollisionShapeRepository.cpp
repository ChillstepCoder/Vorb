#include "stdafx.h"
#include "CollisionShapeRepository.h"

#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btCylinderShape.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"

CollisionShapeRepository::~CollisionShapeRepository()
{
}

btCollisionShape* CollisionShapeRepository::getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents) {
    switch (shapeType) {
        case CollisionShapes::CAPSULE:
            return getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.z);
        case CollisionShapes::CYLINDER:
            return getOrAddCylinderCollisionShape(halfExtents);
        case CollisionShapes::BOX:
            return getOrAddBoxCollisionShape(halfExtents);
        case CollisionShapes::SPHERE:
            return getOrAddSphereCollisionShape(halfExtents.x);
        default:
            assert(false);
            break;

    }
    static_assert(e_cast(CollisionShapes::COUNT) == 4, "Handle new shapes");
    return nullptr;
}

btCollisionShape* CollisionShapeRepository::getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight) {
    // Find existing
    const f32v2 key(radius, halfHeight);
    auto&& it = mCapsuleShapes.find(key);
    if (it != mCapsuleShapes.end()) {
        return it->second.get();
    }

    // Insert new
    std::unique_ptr<btCollisionShape> newShape = std::make_unique<btCapsuleShapeZ>(radius, halfHeight * 2.0f);
    btCollisionShape* rv = newShape.get();
    mCapsuleShapes[key] = std::move(newShape);
    return rv;
}

btCollisionShape* CollisionShapeRepository::getOrAddCylinderCollisionShape(const f32v3& halfExtents) {
    // Find existing
    auto&& it = mCylinderShapes.find(halfExtents);
    if (it != mCylinderShapes.end()) {
        return it->second.get();
    }

    // Insert new
    std::unique_ptr<btCollisionShape> newShape = std::make_unique<btCylinderShapeZ>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btCollisionShape* rv = newShape.get();
    mCylinderShapes[halfExtents] = std::move(newShape);
    return rv;
}

btCollisionShape* CollisionShapeRepository::getOrAddBoxCollisionShape(const f32v3& halfExtents) {
    // Find existing
    auto&& it = mBoxShapes.find(halfExtents);
    if (it != mBoxShapes.end()) {
        return it->second.get();
    }

    // Insert new
    std::unique_ptr<btCollisionShape> newShape = std::make_unique<btBoxShape>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btCollisionShape* rv = newShape.get();
    mBoxShapes[halfExtents] = std::move(newShape);
    return rv;
}

btCollisionShape* CollisionShapeRepository::getOrAddSphereCollisionShape(f32 radius) {
    // Find existing
    auto&& it = mSphereShapes.find(radius);
    if (it != mSphereShapes.end()) {
        return it->second.get();
    }

    // Insert new
    std::unique_ptr<btCollisionShape> newShape = std::make_unique<btSphereShape>(radius);
    btCollisionShape* rv = newShape.get();
    mSphereShapes[radius] = std::move(newShape);
    return rv;
}
