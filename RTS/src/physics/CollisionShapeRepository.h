#pragma once

#include "physics/CollisionShapes.h"

class btCollisionShape;

class CollisionShapeRepository {
public:
    CollisionShapeRepository() = default;
    ~CollisionShapeRepository();

    CollisionShapeID getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents);
    CollisionShapeID getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight);
    CollisionShapeID getOrAddCylinderCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddBoxCollisionShape(const f32v3& halfExtents);
    CollisionShapeID getOrAddSphereCollisionShape(f32 radius);
    btCollisionShape* getShape(CollisionShapeID id) const { return mAllShapes[id].get(); }

private:
    std::unordered_map<f32v2 /*radius, halfHeight*/, CollisionShapeID, f32v2hash> mCapsuleShapes;
    std::unordered_map<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mCylinderShapes;
    std::unordered_map<f32v3 /*xyz halfExtents*/, CollisionShapeID, f32v3hash> mBoxShapes;
    std::unordered_map<f32 /*radius*/, CollisionShapeID> mSphereShapes;
    std::vector<std::unique_ptr<btCollisionShape>> mAllShapes;

    static_assert(e_cast(CollisionShapes::COUNT) == 4, "Track new shapes");
};

