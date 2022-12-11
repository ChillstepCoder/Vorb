#include "stdafx.h"
#include "CollisionShapeRepository.h"

#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btCylinderShape.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"

CollisionShapeRepository::~CollisionShapeRepository()
{
}

CollisionShapeID CollisionShapeRepository::getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents) {
    assert(IS_GAME_THREAD() || IS_RENDER_THREAD()); // Render thread does this on startup
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
    const CollisionShapeID id = mAllShapes.size();
    mAllShapes.emplace_back(std::make_unique<btCapsuleShapeZ>(radius, halfHeight * 2.0f));
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
    const CollisionShapeID id = mAllShapes.size();
    mAllShapes.emplace_back(std::make_unique<btCylinderShapeZ>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z)));
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
    const CollisionShapeID id = mAllShapes.size();
    mAllShapes.emplace_back(std::make_unique<btBoxShape>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z)));
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
    const CollisionShapeID id = mAllShapes.size();
    mAllShapes.emplace_back(std::make_unique<btSphereShape>(radius));
    mSphereShapes[radius] = id;
    return id;
}