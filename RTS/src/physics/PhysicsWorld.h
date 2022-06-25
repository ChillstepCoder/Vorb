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

#include "world/TerrainConstants.h"


class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void stepSimulation(f32 deltaTime);
    DynamicCharacterController* addDynamicCharacterController(const f32v3& position, f32 rotationYaw);
    btRigidBody* addHeightField(const HeightmapPatch& patch);
    btRigidBody* addRigidBody(entt::entity entityOwner, const f32v3& position, CollisionShapes shape, f32 mass, f32v3 scale = f32v3(1.0f));

    void debugRender() const;

private:
    btRigidBody* createRigidBody(btScalar mass, const btTransform& startTransform, btCollisionShape* shape);

    std::unique_ptr<btDefaultCollisionConfiguration> mCollisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> mDispatcher;
    std::unique_ptr<btBroadphaseInterface> mOverlappingPairCache;
    std::unique_ptr<btSequentialImpulseConstraintSolver> mSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> mDynamicsWorld;

    std::vector<btCollisionShape*> mShapes;

    std::unique_ptr<PhysicsDebugDrawer> mDebugDrawer;
    mutable bool mWasDebugRendering = false;

    std::unique_ptr<btHeightfieldTerrainShape> mHeightShapes[WORLD_SIZE_HEIGHTMAP_PATCHES];

};

