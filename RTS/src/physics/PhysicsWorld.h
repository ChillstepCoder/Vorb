#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;
class StaticPhysicsMeshBuilder;
class TrackedStaticModelColliderGatherer;
class HeightmapPatch;

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include "physics/PhysHitResult.h"
#include "physics/CollisionShapes.h"
#include "physics/PhysicsBodyUserData.h"
#include "physics/BroadphaseLayers.h"
#include "PhysicsObjectLayer.h"
#include "physics/PhysicsWorldEvents.h"

#define ENABLE_PHYSICS_ANALYTICS 1

class Camera3D;
class CollisionShapeRepository;

namespace JPH {
    class Body;
    class CharacterBase;
    class Character;
    class BodyLockInterface;
    class BodyInterface;
    class MeshShapeSettings;
    class ObjectLayerFilter;
}

struct NewTileContainerPhysicsData {
    UnorderedFlatMap<TileKey, PhysBodyID> mTileKeyToPhysBodyID;
    PhysBodyID mStaticMesh = INVALID_PHYS_BODY_ID;
};

class PhysicsWorldBodyInterface;
class PhysicsBodyActivationListener;

class PhysicsWorld {
    friend class PhysicsWorldBodyInterface;
    friend class PhysicsBodyActivationListener;
public:
    PhysicsWorld(World& world, CollisionShapeRepository& shapeRepo);
    ~PhysicsWorld();

    static void initializeJPH();

    // Returns number of steps taken
    int stepSimulation(f32 deltaTime);

    // ===========================================================================
    // Body Creation
    // ===========================================================================

    void updateTerrainBody(HeightmapPatch& patch);

    PhysBodyID createDynamicItemBody(entt::entity ownerEntity, f32v3 position, CollisionShapeID shapeId, glm::quat orientation, f32v3 linearVelocity, f32v3 angularVelocity);
    PhysBodyID createStaticItemBody(entt::entity ownerEntity, f32v3 position, CollisionShapeID shapeId, glm::quat orientation, f32 scale);
    PhysBodyID createCharacterBody(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);
    std::unique_ptr<JPH::CharacterBase> createSimpleCharacter(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);

    void changeStaticItemBodyScale(PhysBodyID id, f32 scale);

    void updateTileContainerMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder);

    // This will also clear user data from the body
    void removeBody(PhysBodyID id, bool shouldDestroy);

    // ===========================================================================
    // Queries
    // ===========================================================================
    // TODO: Separate queries into its own interface class?
    // For filters see "physics/PhysicsBroadPhaseLayerFilters.h" "physics/PhysicObjectLayerFilters.h" "physics/PhysicsBodyFilters.h"
    PhysHitResult raycastFirst(
        f32v3 rayStart,
        f32v3 rayEnd,
        const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter = {},
        const JPH::ObjectLayerFilter& objectLayerFilter = {},
        const JPH::BodyFilter& bodyFilter = {},
        bool traceFarTerrain = false
    ) const;
    int queryObjectsInAABB(
        f32v3 min,
        f32v3 max,
        std::span<PhysicsQueryResult> outResults,
        const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter = {},
        const JPH::ObjectLayerFilter& objectLayerFilter = {},
        const JPH::BodyFilter& bodyFilter = {}
    );
    int collideSphere(
        f32v3 center,
        f32 radius,
        std::span<PhysHitResult> outResults,
        const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter = {},
        const JPH::ObjectLayerFilter& objectLayerFilter = {},
        const JPH::BodyFilter& bodyFilter = {}
    );
    int collideCylinder(
        f32v3 center,
        f32v2 halfDims,
        std::span<PhysHitResult> outResults,
        const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter = {},
        const JPH::ObjectLayerFilter& objectLayerFilter = {},
        const JPH::BodyFilter& bodyFilter = {}
    );

    // ===========================================================================
    // Events
    // ===========================================================================
    EVENT_LISTENER_FUNCS(PhysicsWorld, ItemAtRest, PhysicsWorldEventType::ItemAtRest, PhysicsWorldEvent&);
    EVENT_LISTENER_FUNCS(PhysicsWorld, ItemMoved, PhysicsWorldEventType::ItemMoved, PhysicsWorldEvent&);

    // ===========================================================================
    // Debugging
    // ===========================================================================
    void updateAndRenderImguiDebugControls();
    void debugRender(const Camera3D& camera) const;

#if ENABLE_PHYSICS_ANALYTICS == 1
    int getBodyCount(JPH::ObjectLayer layer) const;
#endif

private:
    JPH::ObjectLayer makeObjectLayerMasked(JPH::ObjectLayer layerBits, JPH::ObjectLayer collideMaskBits);

    // ===========================================================================
    // Private API
    // ===========================================================================
    JPH::BodyCreationSettings makeBodyCreateSettings(f32v3 position, CollisionShapeID shapeId, JPH::EMotionType motionType, JPH::ObjectLayer layer, f32 scale);
    PhysBodyID createEntityBody(const JPH::BodyCreationSettings& createSettings, entt::entity ownerEntity, CollisionShapeID shapeId);
    PhysBodyID createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, f32q orientation, ModelID modelId, f32 scale);
    PhysBodyID createTerrainBody(f32v3 position, JPH::Shape* terrainShape);
    JPH::MeshShapeSettings createStaticMeshShapeSettings(std::span<f32v3> verts, std::span<ui32> indices);
    void addTrackedStaticRigidBodiesFromGatherer(TrackedStaticModelColliderGatherer& gatherer, NewTileContainerPhysicsData& physicsData);
    void updateTrackedStaticRigidBodiesFromGatherer(TrackedStaticModelColliderGatherer& gatherer, NewTileContainerPhysicsData& physicsData);
    void updateItemEntitiesChangedThisFrame();

    // ===========================================================================
    // PhysicsWorldBodyInterface
    // ===========================================================================
    // Use this to query the body in a thread safe manner
    const JPH::BodyLockInterface& getBodyLockInterface() const;
    // Use this when bodyLockInterface does not suffice due to broadphase manipulation (mutating positions)
    JPH::BodyInterface& getBodyInterface() const;
    // Use this on game thread as we only mutate bodies on game thread
    const JPH::BodyInterface& getBodyInterfaceNonLocking() const;

    // ===========================================================================
    // Data
    // ===========================================================================
    World& mWorld;
    CollisionShapeRepository& mShapeRepo;
    std::unique_ptr<JPHPhysicsWorldContext> mContext;

    f32 mTickTimeRemainder = 0.0f;

    UnorderedFlatMap<TileContainerID, std::unique_ptr<NewTileContainerPhysicsData>> mTileContainerPhysicsData;

    std::mutex mItemEntitiesMutex;
    // TODO: Profile vs FlatSet and Vector? Vector may be faster for small N
    UnorderedFlatSet<entt::entity> mItemEntitiesRestedThisFrame;
    UnorderedFlatSet<entt::entity> mItemEntitiesMovedThisFrame;

    EVENT_DISPATCHER_DEF(PhysicsWorld);

};

extern PhysicsWorld* sGamePhysicsWorld;


// Trusted classes may use these private members
class PhysicsWorldBodyInterface {
    friend class PhysicsComponent; // TRUSTED

    PhysicsWorldBodyInterface() = delete;
    // Use this to query the body in a thread safe manner
    inline static const JPH::BodyLockInterface& getBodyLockInterface(PhysicsWorld& physicsWorld) { return physicsWorld.getBodyLockInterface(); }
    // Use this when bodyLockInterface does not suffice due to broadphase manipulation (mutating positions)
    inline static JPH::BodyInterface& getBodyInterface(PhysicsWorld& physicsWorld) { return physicsWorld.getBodyInterface(); }
    // Use this on game thread as we only mutate bodies on game thread
    inline static const JPH::BodyInterface& getBodyInterfaceNonLocking(PhysicsWorld& physicsWorld) { return physicsWorld.getBodyInterfaceNonLocking(); }
};
