#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;
class StaticPhysicsMeshBuilder;
class TrackedStaticRigidBodyGatherer;

#include "physics/CollisionShapes.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#define ENABLE_PHYSICS_ANALYTICS 1

class Camera3D;
class CollisionShapeRepository;

namespace JPH {
    class Body;
}

enum class PhysicsObjectLayer : JPH::ObjectLayer {
    Static,
    Dynamic,
    COUNT
};

struct NewTileContainerPhysicsData {
    UnorderedFlatMap<TileKey, PhysBodyID> mTileKeyToPhysBodyID;
    //StaticPhysicsMesh mStaticMesh; // TODO: hmmm
};

enum class PhysicsBodyUserDataType : ui8 {
    Unknown,
    Tile,
    Entity,
    // ?
    COUNT
};
static_assert(e_count(PhysicsBodyUserDataType) <= 4); // Fitting into 2 bits

struct alignas(8) PhysicsBodyUserData {
    PhysicsBodyUserData() = default;
    PhysicsBodyUserData(entt::entity owner);
    PhysicsBodyUserData(TileContainerID tileContainer, TileIndex tileIndex);
    PhysicsBodyUserData(ui64 data) : data(data) {}

    operator ui64() const { return std::bit_cast<ui64>(*this); }
    PhysicsBodyUserDataType getType() const;

    // Only works if getType == Tile
    std::pair<TileContainerID, TileIndex> getTileData() const;
    // Only works if getType == Entity
    entt::entity getEntity() const;

    // We use bit packing to store lots of data in here
    ui64 data = 0;
};
static_assert(sizeof(PhysicsBodyUserData) == sizeof(ui64), "JoltUserData must be 64 bits");

class NewPhysicsWorld {
public:
    NewPhysicsWorld(World& world, CollisionShapeRepository& shapeRepo);
    ~NewPhysicsWorld();

    static void initializeJPH();

    // Returns number of steps taken
    int stepSimulation(f32 deltaTime);

    // ===========================================================================
    // Body Creation
    // ===========================================================================

    PhysBodyID createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);

    void updateTileContainerMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder);

    // ===========================================================================
    // Debugging
    // ===========================================================================
    void debugRender(const Camera3D& camera) const;

#if ENABLE_PHYSICS_ANALYTICS == 1
    int getBodyCount(PhysicsObjectLayer layer) const;
#endif

private:
    JPH::Body& createBodyInternal(f32v3 position, CollisionShapeID shapeId, PhysicsObjectLayer layer);
    PhysBodyID createEntityBody(entt::entity ownerEntity, f32v3 position, CollisionShapeID shapeId, PhysicsObjectLayer layer);
    PhysBodyID createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, CollisionShapeID shapeId);
    void addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, NewTileContainerPhysicsData& physicsData);

    World& mWorld;
    CollisionShapeRepository& mShapeRepo;
    std::unique_ptr<JPHPhysicsWorldContext> mContext;

    f32 mTickTimeRemainder = 0.0f;

    UnorderedFlatMap<TileContainerID, std::unique_ptr<NewTileContainerPhysicsData>> mTileContainerPhysicsData;
};

