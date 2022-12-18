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

constexpr int INVALID_PHYSICS_USER_INDEX = INT32_MAX;

#include "world/TerrainConstants.h"

#include "physics/PhysHitResult.h"
#include "physics/CollisionShapes.h"
#include "physics/StaticPhysicsMesh.h"
#include "physics/CollisionShapeRepository.h"
#include "physics/TrackedStaticRigidBodyGatherer.h"
#include "tile/TileContainerEvents.h"

#include <shared_mutex>

enum class RigidBodyRotationType {
    FULL,
    NO_ROTATE,
    NO_ROTATE_XY,
};

typedef std::pair<btRigidBody*, f32/*colliderHalfHeight*/> RigidBodyPair;
typedef std::map<TileIndex, btCollisionObject*> SpatialCollisionObjectLookup;

enum PickTypes {
    PICK_TYPE_STATIC = 1 << 0,
    PICK_TYPE_DYNAMIC = 1 << 1,
    PICK_TYPE_ALL = PICK_TYPE_STATIC | PICK_TYPE_DYNAMIC
};

struct TileContainerPhysicsData {
    SpatialCollisionObjectLookup mSpatialCollisionObjectLookup;
    StaticPhysicsMesh mStaticMesh; // TODO: hmmm
};

struct PickParams {
    f32v3 rayStart;
    f32v3 rayEnd;
    PickTypes pickTypes;
};

class PhysicsWorld
{
public:
    PhysicsWorld(CollisionShapeRepository& shapeRepository);
    ~PhysicsWorld();

    void stepSimulation(f32 deltaTime);

    DynamicCharacterController* addDynamicCharacterController(entt::entity ownerEntity, btRigidBody* rigidBody, f32 rotationYaw);
    btCollisionObject* addHeightField(const HeightmapPatch& patch);
    void deleteHeightField(HeightmapPatch& patch);

    RigidBodyPair addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shapeType, const f32v3& halfExtents, f32 mass, RigidBodyRotationType rotationType = RigidBodyRotationType::FULL);
    RigidBodyPair addRigidBody(entt::entity ownerEntity, const f32v3& position, btCollisionShape* collisionShape, f32 mass, RigidBodyRotationType rotationType = RigidBodyRotationType::FULL);

    void addTrackedStaticCollisionObjectAtPosition(TileContainerID containerOwner, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* collisionShape);
    void removeTrackedStaticCollisionObjectAtPosition(TileContainerID containerId, TileIndex tileIndex);
    void deletePhysicsForTileContainer(TileContainerID container);

    void deleteRigidBody(btRigidBody* rigidBody);
    void deleteStaticPhysicsMesh(StaticPhysicsMesh&& physicsMesh);
    void addStaticMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder);

    void debugRender() const;

    // Picking
    PhysHitResult pick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes) const;
    void pickDeferred(DeferredPhysicsPick* deferredPick, const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes);
    // Returns false if the physics is currently locked by the game thread
    bool tryPick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes, OUT PhysHitResult& result) const;

    CollisionShapeRepository& getShapeRepository() { return mShapeRepository; }

    ui32 getNumStaticCollisionObjects() const { return mNumStaticCollisionObjects; }
    ui32 getNumDynamicCollisionObjects() const { return mNumDynamicCollisionObjects; }

    // Create profile file for chrome://tracing/
    // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview
    void startB3Profiling();
    void endB3ProfilingAndDumpToFile(const char* fileNamePrefix);
    bool isProfiling() const { return mIsProfiling; }

private:
    void initEventHandlers();
    void addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, SpatialCollisionObjectLookup& lookup);
    RigidBodyPair createRigidBody(entt::entity ownerEntity, btScalar mass, const f32v3& position, btCollisionShape* shape);
    btCollisionObject* createStaticCollisionObject(TileContainerID ownerTileContainer, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* shape);
    f32 getShapeHalfHeight(btCollisionShape* shape) const;
    btCollisionObject* allocStaticCollisionObject();
    void freeStaticCollisionObject(btCollisionObject* obj);

    std::unique_ptr<btDefaultCollisionConfiguration> mCollisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> mDispatcher;
    std::unique_ptr<btBroadphaseInterface> mOverlappingPairCache;
    std::unique_ptr<btSequentialImpulseConstraintSolver> mSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> mDynamicsWorld;

    CollisionShapeRepository& mShapeRepository;

    // Deferred picking
    moodycamel::ConcurrentQueue<std::pair<PickParams, DeferredPhysicsPick*>> mDeferredPicks;

    // Synchronization
    mutable std::shared_mutex mStepSimulationMutex;

    // Events
    TileContainerListeners mTileContainerEventListeners;

    // For cleanup
    std::map<HeightmapPatchData*, btHeightfieldTerrainShape*> mHeightShapes;

    std::vector<btCollisionObject*> mFreeStaticCollisionObjects;
    moodycamel::ConcurrentQueue<StaticPhysicsMesh> mStaticPhysicsMeshesToDelete;

    std::map<TileContainerID, TileContainerPhysicsData> mTileContainerPhysicsData; // Model colliders and such

    // Debug drawing
    std::unique_ptr<PhysicsDebugDrawer> mDebugDrawer;
    mutable bool mWasRenderingStatic = false;
    mutable bool mWasRenderingTerrain = false;

    ui32 mNumStaticCollisionObjects = 0;
    ui32 mNumDynamicCollisionObjects = 0;

    std::atomic_bool mIsProfiling = false;

};

