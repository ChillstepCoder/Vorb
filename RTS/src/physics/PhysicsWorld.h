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
class StaticPhysicsMesh;

#include "world/TerrainConstants.h"

#include "physics/CollisionShapes.h"

enum class RigidBodyRotationType {
    FULL,
    NO_ROTATE,
    NO_ROTATE_XY,
};

typedef std::pair<btRigidBody*, f32/*colliderHalfHeight*/> RigidBodyPair;

class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void stepSimulation(f32 deltaTime);
    DynamicCharacterController* addDynamicCharacterController(entt::entity ownerEntity, btRigidBody* rigidBody, f32 rotationYaw);
    btRigidBody* addHeightField(const HeightmapPatch& patch);
    RigidBodyPair addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shape, f32 mass, f32v3 scale = f32v3(1.0f), RigidBodyRotationType rotationType = RigidBodyRotationType::FULL);

    void addStaticMesh(StaticPhysicsMesh& staticMesh);

    void debugRender() const;

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

    std::unique_ptr<btHeightfieldTerrainShape> mHeightShapes[WORLD_SIZE_HEIGHTMAP_PATCHES];

};

