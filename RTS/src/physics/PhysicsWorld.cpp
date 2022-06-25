#include "stdafx.h"
#include "PhysicsWorld.h"

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
//#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "debugging/PhysicsDebugDrawer.h"

#include "physics/DynamicCharacterController.h"

#include "physics/CollisionShapes.h"

#include "terrain/HeightmapPatch.h"
#include "options/DebugOptions.h"

const btVector3 GRAVITY(0.0f, 0.0f, -10.0f);

const btVector3 DEBUG_COLOR_DYNAMIC(0.0, 1.0, 0.0);
const btVector3 DEBUG_COLOR_STATIC(1.0, 0.0, 0.0);

PhysicsWorld::PhysicsWorld() {
    /// collision configuration contains default setup for memory , collision setup . Advanced users can create their own configuration .
    mCollisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    /// use the default collision dispatcher . For parallel processing you can use a diffent dispatcher(see Extras / BulletMultiThreaded)
    mDispatcher = std::make_unique<btCollisionDispatcher>(mCollisionConfiguration.get());
    /// btDbvtBroadphase is a good general purpose broadphase . You can also try out btAxis3Sweep .
    mOverlappingPairCache = std::make_unique<btDbvtBroadphase>();
    /// the default constraint solver . For parallel processing you can use a different solver  (see Extras / BulletMultiThreaded)
    mSolver = std::make_unique<btSequentialImpulseConstraintSolver>();

    mDebugDrawer = std::make_unique<PhysicsDebugDrawer>();

    mDynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(mDispatcher.get(), mOverlappingPairCache.get(), mSolver.get(), mCollisionConfiguration.get());
    mDynamicsWorld->setGravity(GRAVITY);
    mDynamicsWorld->setDebugDrawer(mDebugDrawer.get()); // TODO: Don't do this on release builds?

    // ========================= Create all shapes =========================
    mShapes.resize(e_cast(CollisionShapes::COUNT));
    {
        mShapes[e_cast(CollisionShapes::CAPSULE)] = new btCapsuleShapeZ(0.25f /*radius*/, 1.0f /*height*/);
    }

    static_assert(e_cast(CollisionShapes::COUNT) == 1, "Update new shapes");
}

PhysicsWorld::~PhysicsWorld() {
    //cleanup in the reverse order of creation/initialization

    //remove the rigidbodies from the dynamics world and delete them
    int i;
    for (i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
        btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
        btRigidBody* body = btRigidBody::upcast(obj);
        if (body && body->getMotionState())
        {
            while (body->getNumConstraintRefs())
            {
                btTypedConstraint* constraint = body->getConstraintRef(0);
                mDynamicsWorld->removeConstraint(constraint);
                delete constraint;
            }
            delete body->getMotionState();
            mDynamicsWorld->removeRigidBody(body);
        }
        else
        {
            mDynamicsWorld->removeCollisionObject(obj);
        }
        delete obj;
    }

    //delete collision shapes
   /* for (int j = 0; j < m_collisionShapes.size(); j++)
    {
        btCollisionShape* shape = m_collisionShapes[j];
        delete shape;
    }
    m_collisionShapes.clear();*/
}

void PhysicsWorld::stepSimulation(f32 elapsedSec) {
    mDynamicsWorld->stepSimulation(elapsedSec, 3 /*maxSubSteps*/);
}

DynamicCharacterController* PhysicsWorld::addDynamicCharacterController(const f32v3& position, f32 rotationYaw) {
    btTransform startTransform;
    startTransform.setOrigin(btVector3(position.x, position.y, position.z));
    startTransform.setRotation(btQuaternion(rotationYaw, 0.0, 0.0));

    // TODO: Custom btAction character controller
    // https://www.pierov.org/2020/05/23/dynamic-character-controller-bullet/
    btCapsuleShape* shape = (btCapsuleShape*)mShapes[e_cast(CollisionShapes::CAPSULE)];
    btRigidBody* body = createRigidBody(1.0f, startTransform, shape);
    body->setAngularFactor(0.0);
    body->setSleepingThresholds(0.0, 0.0);

    DynamicCharacterController* dynamicCharacterController = new DynamicCharacterController(body, shape);
    mDynamicsWorld->addAction(dynamicCharacterController);
    return dynamicCharacterController;
}

btRigidBody* PhysicsWorld::addHeightField(const HeightmapPatch& patch)
{
    btTransform startTransform;
    const f32v3 center = patch.mHeightData->aabb.getCenter();
    startTransform.setOrigin(btVector3(center.x, center.y, center.z));
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));

    // TODO: Store this properly instead of leaking it..
    btHeightfieldTerrainShape* heightFieldShape = new btHeightfieldTerrainShape(
        HEIGHTMAP_VERT_WIDTH_PER_PATCH,
        HEIGHTMAP_VERT_WIDTH_PER_PATCH,
        patch.mHeightData->data,
        patch.mHeightData->aabb.z,
        patch.mHeightData->aabb.getMaxZ(),
        AXIS_Z,
        false /*flipQuadEdges*/
    );
    heightFieldShape->setUseDiamondSubdivision();
    heightFieldShape->setLocalScaling(btVector3(HEIGHTMAP_QUAD_SIZE, HEIGHTMAP_QUAD_SIZE, 1.0f));
    return createRigidBody(0.0f, startTransform, heightFieldShape);
}

btRigidBody* PhysicsWorld::addRigidBody(entt::entity entityOwner, const f32v3& position, CollisionShapes shape, f32 mass, f32v3 scale /*= f32v3(1.0f)*/) {
    btTransform startTransform;
    startTransform.setOrigin(btVector3(position.x, position.y, position.z));
    //startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    btRigidBody* rigidBody = createRigidBody(mass, startTransform, s
}

btRigidBody* PhysicsWorld::createRigidBody(btScalar mass, const btTransform& startTransform, btCollisionShape* shape)
{
    btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));
    
    btVector3 localInertia(0, 0, 0);
    btRigidBody* body;
    if (mass) {
        // Nonzero mass, dynamic body
        shape->calculateLocalInertia(mass, localInertia);

        // Using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo cInfo(mass, myMotionState, shape, localInertia);
        body = new btRigidBody(cInfo);
        //body->setContactProcessingThreshold(m_defaultContactProcessingThreshold);
    }
    else {
        // Zero mass, static body
        body = new btRigidBody(mass, 0, shape, localInertia);
        body->setWorldTransform(startTransform);
    }

    body->setUserIndex(-1);
    mDynamicsWorld->addRigidBody(body);
    return body;
}

void PhysicsWorld::debugRender() const {
    if (sDebugOptions.mShowPhysicsDebug) {
        if (!mWasDebugRendering) {
            mWasDebugRendering = true;
            // Draw static and dynamic
            ScopedTimer timer("Static debug");
            mDebugDrawer->reserveStaticLines(2000000);
            for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
                btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body->getMass()) {
                    mDebugDrawer->setIsStaticMode(false);
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
                }
                else {
                    mDebugDrawer->setIsStaticMode(true);
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_STATIC);
                }
            }
        }
        else {
            // Draw only dynamic
            mDebugDrawer->setIsStaticMode(false);
            for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
                btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body->getMass()) {
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
                }
            }
        }
        // Draw actions
        const auto& actions = mDynamicsWorld->getActions();
        for (int i = 0; i < actions.size(); ++i) {
            actions[i]->debugDraw(mDebugDrawer.get());
        }
    }
    else {
        if (mWasDebugRendering) {
            mWasDebugRendering = false;
            // Disable static geometry
            mDebugDrawer->clearStaticLines();
        }
    }
    //mDynamicsWorld->debugDrawWorld();
    // Dont debug terrain as it is terribly slow
    
}
