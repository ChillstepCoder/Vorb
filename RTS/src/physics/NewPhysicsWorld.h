#pragma once
class World;

class JPHPhysicsWorldContext;
class HeightmapPatch;

#include "physics/CollisionShapes.h"

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

