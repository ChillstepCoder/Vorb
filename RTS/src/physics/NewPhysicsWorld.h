#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;

#include "physics/CollisionShapes.h"

using PhysBodyID = ui32;

enum class RigidBodyRotationType {
    FULL,
    NO_ROTATE,
    NO_ROTATE_XY,
};

class NewPhysicsWorld {
public:
    NewPhysicsWorld(World& world);
    ~NewPhysicsWorld();

    // Returns number of steps taken
    int stepSimulation(f32 deltaTime);

    PhysBodyID createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents);

    void debugRender() const;

private:

    World& mWorld;
    std::unique_ptr<JPHPhysicsWorldContext> mContext;

    f32 mTickTimeRemainder = 0.0f;
};

