#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;
class StaticPhysicsMeshBuilder;
class TrackedStaticRigidBodyGatherer;
class HeightmapPatch;


#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>


#include "physics/CollisionShapes.h"
#include "physics/StaticPhysicsMesh.h"

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
}

enum class PhysicsObjectLayer : JPH::ObjectLayer {
    Static,
    Dynamic,
    COUNT
};

struct NewTileContainerPhysicsData {
    UnorderedFlatMap<TileKey, PhysBodyID> mTileKeyToPhysBodyID;
    StaticPhysicsMesh mStaticMesh;
};

enum class PhysicsBodyUserDataType : ui8 {
    Terrain,
    Tile,
    Entity,
    ContainerMesh,
    COUNT
};
static_assert(e_count(PhysicsBodyUserDataType) <= 4); // Fitting into 2 bits

struct alignas(8) PhysicsBodyUserData {
    PhysicsBodyUserData() = default;
    PhysicsBodyUserData(entt::entity owner);
    PhysicsBodyUserData(TileContainerID tileContainer);
    PhysicsBodyUserData(TileContainerID tileContainer, TileIndex tileIndex);
    PhysicsBodyUserData(ui64 data) : data(data) {}

    operator ui64() const { return std::bit_cast<ui64>(*this); }
    PhysicsBodyUserDataType getType() const;

    // Only works if getType == Tile
    std::pair<TileContainerID, TileIndex> getTileData() const;

    // Only works if getType == Tile || ContainerMesh
    TileContainerID getContainerId() const;

    // Only works if getType == Entity
    entt::entity getEntity() const;

    // We use bit packing to store lots of data in here
    ui64 data = 0;
};
static_assert(sizeof(PhysicsBodyUserData) == sizeof(ui64), "JoltUserData must be 64 bits");

class PhysicsWorldBodyInterface;

class NewPhysicsWorld {
    friend class PhysicsWorldBodyInterface;
public:
    NewPhysicsWorld(World& world, CollisionShapeRepository& shapeRepo);
    ~NewPhysicsWorld();

    static void initializeJPH();

    // Returns number of steps taken
    int stepSimulation(f32 deltaTime);

    // ===========================================================================
    // Body Creation
    // ===========================================================================

    void updateTerrainBody(HeightmapPatch& patch);

    PhysBodyID createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);
    std::unique_ptr<JPH::CharacterBase> createSimpleCharacter(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);

    void updateTileContainerMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder);

    void removeBody(PhysBodyID id);

    // ===========================================================================
    // Debugging
    // ===========================================================================
    void updateAndRenderImguiDebugControls();
    void debugRender(const Camera3D& camera) const;

#if ENABLE_PHYSICS_ANALYTICS == 1
    int getBodyCount(PhysicsObjectLayer layer) const;
#endif

private:
    // ===========================================================================
    // Private API
    // ===========================================================================
    JPH::BodyCreationSettings makeBodyCreateSettings(f32v3 position, CollisionShapeID shapeId, JPH::EMotionType motionType, PhysicsObjectLayer layer);
    PhysBodyID createEntityBody(const JPH::BodyCreationSettings& createSettings, entt::entity ownerEntity, CollisionShapeID shapeId);
    PhysBodyID createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, CollisionShapeID shapeId);
    PhysBodyID createTerrainBody(f32v3 position, const JPH::Shape* terrainShape);
    JPH::MeshShapeSettings createStaticMeshShapeSettings(std::span<f32v3> verts, std::span<ui32> indices);
    void addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, NewTileContainerPhysicsData& physicsData);
    void updateTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, NewTileContainerPhysicsData& physicsData);

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
};

extern NewPhysicsWorld* sGamePhysicsWorld;


// Trusted classes may use these private members
class PhysicsWorldBodyInterface {
    friend class PhysicsComponent; // TRUSTED

    PhysicsWorldBodyInterface() = delete;
    // Use this to query the body in a thread safe manner
    inline static const JPH::BodyLockInterface& getBodyLockInterface(NewPhysicsWorld& physicsWorld) { return physicsWorld.getBodyLockInterface(); }
    // Use this when bodyLockInterface does not suffice due to broadphase manipulation (mutating positions)
    inline static JPH::BodyInterface& getBodyInterface(NewPhysicsWorld& physicsWorld) { return physicsWorld.getBodyInterface(); }
    // Use this on game thread as we only mutate bodies on game thread
    inline static const JPH::BodyInterface& getBodyInterfaceNonLocking(NewPhysicsWorld& physicsWorld) { return physicsWorld.getBodyInterfaceNonLocking(); }
};
