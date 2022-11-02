#pragma once

class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btCollisionShape;
class btRigidBody;
class PhysicsDebugDrawer;
class HeightmapPatch;
class btCollisionShape;
class btTransform;
class btVector4;
class btHeightfieldTerrainShape;
class DynamicCharacterController;
class StaticPhysicsMeshBuilder;
struct HeightmapPatchData;

#include "world/TerrainConstants.h"

#include "physics/PhysHitResult.h"
#include "physics/CollisionShapes.h"

#include <shared_mutex>

enum class RigidBodyRotationType {
    FULL,
    NO_ROTATE,
    NO_ROTATE_XY,
};

typedef std::pair<btRigidBody*, f32/*colliderHalfHeight*/> RigidBodyPair;

enum PickTypes {
    PICK_TYPE_STATIC = 1 << 0,
    PICK_TYPE_DYNAMIC = 1 << 1,
    PICK_TYPE_ALL = PICK_TYPE_STATIC | PICK_TYPE_DYNAMIC
};

class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void stepSimulation(f32 deltaTime);

    DynamicCharacterController* addDynamicCharacterController(entt::entity ownerEntity, btRigidBody* rigidBody, f32 rotationYaw);
    btRigidBody* addHeightField(const HeightmapPatch& patch);
    void deleteHeightField(HeightmapPatch& patch);
    RigidBodyPair addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shape, f32 mass, f32v3 scale = f32v3(1.0f), RigidBodyRotationType rotationType = RigidBodyRotationType::FULL);
    void deleteRigidBody(btRigidBody** rigidBody);
    void addStaticMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder, OUT StaticPhysicsMesh& outMesh);

    void debugRender() const;

    // Picking
    PhysHitResult pick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes) const;
    // Returns false if the physics is currently locked by the game thread
    bool tryPick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes, OUT PhysHitResult& result) const;

private:
    RigidBodyPair createRigidBody(entt::entity ownerEntity, btScalar mass, const btTransform& startTransform, btCollisionShape* shape);

    std::unique_ptr<btDefaultCollisionConfiguration> mCollisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> mDispatcher;
    std::unique_ptr<btBroadphaseInterface> mOverlappingPairCache;
    std::unique_ptr<btSequentialImpulseConstraintSolver> mSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> mDynamicsWorld;

    std::vector<btCollisionShape*> mShapes;

    std::unique_ptr<PhysicsDebugDrawer> mDebugDrawer;
    mutable bool mWasRenderingStatic = false;
    mutable bool mWasRenderingTerrain = false;

    mutable std::shared_mutex mMutex;

    // For cleanup
    std::map<HeightmapPatchData*, btHeightfieldTerrainShape*> mHeightShapes;

    moodycamel::ConcurrentQueue<btRigidBody*> mRigidBodiesToAdd;
    moodycamel::ConcurrentQueue<btRigidBody*> mRigidBodiesToDelete;

};

