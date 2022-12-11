#pragma once

#include "physics/CollisionShapes.h"

struct f32v3Hash {
    std::size_t operator()(const f32v3& k) const {
        return std::hash<f32>()(k.x) ^ std::hash<f32>()(k.y) ^ std::hash<f32>()(k.z);
    }
};

class btCollisionShape;

class CollisionShapeRepository {
public:
    CollisionShapeRepository() = default;
    ~CollisionShapeRepository();

    btCollisionShape* getOrAddCollisionShape(CollisionShapes shapeType, const f32v3& halfExtents);
    btCollisionShape* getOrAddCapsuleCollisionShape(f32 radius, f32 halfHeight);
    btCollisionShape* getOrAddCylinderCollisionShape(const f32v3& halfExtents);
    btCollisionShape* getOrAddBoxCollisionShape(const f32v3& halfExtents);
    btCollisionShape* getOrAddSphereCollisionShape(f32 radius);

private:
    std::unordered_map<f32v2 /*radius, halfHeight*/, std::unique_ptr<btCollisionShape>, f32v2hash> mCapsuleShapes;
    std::unordered_map<f32v3 /*xyz halfExtents*/, std::unique_ptr<btCollisionShape>, f32v3hash> mCylinderShapes;
    std::unordered_map<f32v3 /*xyz halfExtents*/, std::unique_ptr<btCollisionShape>, f32v3hash> mBoxShapes;
    std::unordered_map<f32 /*radius*/, std::unique_ptr<btCollisionShape>> mSphereShapes;
    static_assert(e_cast(CollisionShapes::COUNT) == 4, "Track new shapes");
};

