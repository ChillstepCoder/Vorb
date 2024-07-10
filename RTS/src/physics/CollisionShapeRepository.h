#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "physics/CollisionShapes.h"
#include "physics/ModelColliderShape.h"

namespace JPH {
    class Shape;
}

class CollisionShapeRepository {
public:
    CollisionShapeRepository();
    ~CollisionShapeRepository();

    VORB_NON_COPYABLE(CollisionShapeRepository);

    static CollisionShapeRepository& get();

    CollisionShapeID getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents);
    CollisionShapeID getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight);
    CollisionShapeID getOrAddCylinderCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddBoxCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddSphereCollisionShape(f32 radius);
    CollisionShapeID addCompoundCollisionShape(std::span<const ModelColliderShape> shapes);
    JPH::Shape* getShape(CollisionShapeID id) const { return mAllJoltShapes[id]; }

private:
    UnorderedFlatMap<f32v2 /*radius, halfHeight*/, CollisionShapeID, f32v2hash> mCapsuleShapes;
    UnorderedFlatMap<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mCylinderShapes;
    UnorderedFlatMap<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mBoxShapes;
    UnorderedFlatMap<f32 /*radius*/, CollisionShapeID> mSphereShapes;
    std::vector<JPH::Ref<JPH::Shape>> mAllJoltShapes;

    static_assert(e_cast(CollisionShapes::COUNT) == 7, "Track new shapes");
};

extern CollisionShapeRepository* sCollisionShapeRepository;
