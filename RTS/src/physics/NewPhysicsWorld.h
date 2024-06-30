#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;
class StaticPhysicsMeshBuilder;
class TrackedStaticRigidBodyGatherer;
class HeightmapPatch;

#include "physics/CollisionShapes.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>

#define ENABLE_PHYSICS_ANALYTICS 1

class Camera3D;
class CollisionShapeRepository;

namespace JPH {
    class Body;
    class Character;
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

    void updateTerrainBody(HeightmapPatch& patch);

    PhysBodyID createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);
    std::unique_ptr<JPH::Character> createSimpleCharacter(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);

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
    JPH::BodyCreationSettings makeBodyCreateSettings(f32v3 position, const JPH::Shape* shape, JPH::EMotionType motionType, PhysicsObjectLayer layer);
    JPH::BodyCreationSettings makeBodyCreateSettings(f32v3 position, CollisionShapeID shapeId, JPH::EMotionType motionType, PhysicsObjectLayer layer);
    PhysBodyID createEntityBody(const JPH::BodyCreationSettings& createSettings, entt::entity ownerEntity, CollisionShapeID shapeId);
    PhysBodyID createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, CollisionShapeID shapeId);
    PhysBodyID createTerrainBody(f32v3 position, const JPH::Shape* terrainShape);
    void addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, NewTileContainerPhysicsData& physicsData);

    World& mWorld;
    CollisionShapeRepository& mShapeRepo;
    std::unique_ptr<JPHPhysicsWorldContext> mContext;

    f32 mTickTimeRemainder = 0.0f;

    UnorderedFlatMap<TileContainerID, std::unique_ptr<NewTileContainerPhysicsData>> mTileContainerPhysicsData;
};

