#pragma once

#include "physics/CollisionShapes.h"

namespace JPH {
    class ShapeSettings;
}

class btCollisionShape;

class CollisionShapeRepository {
public:
    CollisionShapeRepository();
    ~CollisionShapeRepository();

    VORB_NON_COPYABLE(CollisionShapeRepository);

    CollisionShapeID getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents);
    CollisionShapeID getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight);
    CollisionShapeID getOrAddCylinderCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddBoxCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddSphereCollisionShape(f32 radius);
    btCollisionShape* getShape(CollisionShapeID id) const { return mAllShapes[id].get(); }
    JPH::ShapeSettings& getJoltShapeSettings(CollisionShapeID id) const { return *mAllJoltShapes[id]; }

private:
    UnorderedFlatMap<f32v2 /*radius, halfHeight*/, CollisionShapeID, f32v2hash> mCapsuleShapes;
    UnorderedFlatMap<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mCylinderShapes;
    UnorderedFlatMap<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mBoxShapes;
    UnorderedFlatMap<f32 /*radius*/, CollisionShapeID> mSphereShapes;
    std::vector<std::unique_ptr<btCollisionShape>> mAllShapes;
    std::vector<std::unique_ptr<JPH::ShapeSettings>> mAllJoltShapes;

    static_assert(e_cast(CollisionShapes::COUNT) == 4, "Track new shapes");
};

