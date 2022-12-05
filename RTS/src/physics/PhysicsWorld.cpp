#include "stdafx.h"
#include "PhysicsWorld.h"

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
//#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "debugging/PhysicsDebugDrawer.h"

#include "physics/DynamicCharacterController.h"
#include "physics/CollisionShapes.h"
#include "physics/StaticPhysicsMesh.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "terrain/HeightmapPatch.h"
#include "options/DebugOptions.h"

#include "debugging/DebugRenderer.h"
#include "physics/PhysicsConst.h"

const btVector3 DEBUG_COLOR_DYNAMIC(0.0, 1.0, 0.0);
const btVector3 DEBUG_COLOR_STATIC(1.0, 0.0, 0.0);
const btVector3 DEBUG_COLOR_TERRAIN(1.0, 1.0, 1.0);

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
        mShapes[e_cast(CollisionShapes::CAPSULE)] = new btCapsuleShapeZ(0.24f /*radius*/, 1.1f /*height*/);
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
    assert(IS_GAME_THREAD());
    PROFILE_FUNCTION();
    constexpr int BULK_DEQUEUE_SIZE = 64;
    {
        btRigidBody* rigidBodies[BULK_DEQUEUE_SIZE];

        // Add new rigid bodies
        if (size_t count = mRigidBodiesToAdd.try_dequeue_bulk(rigidBodies, BULK_DEQUEUE_SIZE)) {
            std::lock_guard guard(mMutex);
            for (size_t i = 0; i < count; ++i) {
                mDynamicsWorld->addRigidBody(rigidBodies[i]);
            }
        }
        // Delete expired rigid bodies
        if (size_t count = mRigidBodiesToDelete.try_dequeue_bulk(rigidBodies, BULK_DEQUEUE_SIZE)) {
            std::lock_guard guard(mMutex);
            for (size_t i = 0; i < count; ++i) {
                mDynamicsWorld->removeRigidBody(rigidBodies[i]);
                delete rigidBodies[i];
            }
        }
    }
    {
        // Static physics meshes have other RAII associated data that we must free after deleting their rigid bodies
        StaticPhysicsMesh staticPhysicsMeshes[BULK_DEQUEUE_SIZE];
        if (size_t count = mStaticPhysicsMeshesToDelete.try_dequeue_bulk(staticPhysicsMeshes, BULK_DEQUEUE_SIZE)) {
            std::lock_guard guard(mMutex);
            for (size_t i = 0; i < count; ++i) {
                mDynamicsWorld->removeRigidBody(staticPhysicsMeshes[i].mRigidBody);
                delete staticPhysicsMeshes[i].mRigidBody;
            }
        }
    }

    std::lock_guard guard(mMutex);
    mDynamicsWorld->stepSimulation(elapsedSec, 5 /*maxSubSteps*/);
}

DynamicCharacterController* PhysicsWorld::addDynamicCharacterController(entt::entity ownerEntity, btRigidBody* rigidBody, f32 rotationYaw) {
    assert(IS_GAME_THREAD());
    assert(rigidBody->getCollisionShape()->getShapeType() == BroadphaseNativeTypes::CAPSULE_SHAPE_PROXYTYPE);
    DynamicCharacterController* dynamicCharacterController = new DynamicCharacterController(rigidBody, (btCapsuleShape*)rigidBody->getCollisionShape());
    mDynamicsWorld->addAction(dynamicCharacterController);
    return dynamicCharacterController;
}

btRigidBody* PhysicsWorld::addHeightField(const HeightmapPatch& patch)
{
    assert(IS_GAME_THREAD());
    btTransform startTransform;
    const f32v3 center = patch.mHeightData->aabb.getCenter();
    startTransform.setOrigin(btVector3(center.x, center.y, center.z));
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));

    btHeightfieldTerrainShape* heightFieldShape = new btHeightfieldTerrainShape(
        HEIGHTMAP_VERT_WIDTH_PER_PATCH,
        HEIGHTMAP_VERT_WIDTH_PER_PATCH,
        patch.mHeightData->data,
        patch.mHeightData->aabb.z,
        patch.mHeightData->aabb.getMaxZ(),
        AXIS_Z,
        false /*flipQuadEdges*/
    );
    
    // Store so we dont leak
    assert(mHeightShapes.find(patch.mHeightData) == mHeightShapes.end());
    mHeightShapes[patch.mHeightData] = heightFieldShape;

    heightFieldShape->setUseDiamondSubdivision();
    heightFieldShape->setLocalScaling(btVector3(HEIGHTMAP_QUAD_SIZE, HEIGHTMAP_QUAD_SIZE, 1.0f));

    return createRigidBody(entt::null, 0.0f, startTransform, heightFieldShape).first;
}

void PhysicsWorld::deleteHeightField(HeightmapPatch& patch) {
    auto&& it = mHeightShapes.find(patch.mHeightData);
    assert(it != mHeightShapes.end());
    delete it->second;
    mHeightShapes.erase(it);
    deleteRigidBody(patch.mHeightData->mCollider);
    patch.mHeightData->mCollider = nullptr;
}

RigidBodyPair PhysicsWorld::addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shape, f32 mass, f32v3 scale /*= f32v3(1.0f)*/, RigidBodyRotationType rotationType /*= RigidBodyRotationType::FULL*/) {
    btTransform startTransform;
    startTransform.setOrigin(btVector3(position.x, position.y, position.z));
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    assert(shape != CollisionShapes::NONE);
    btCollisionShape* collisionShape = (btCollisionShape*)mShapes[e_cast(shape)];
    RigidBodyPair rv = createRigidBody(ownerEntity, mass, startTransform, collisionShape);

    // Disable rotation optionally
    if (rotationType == RigidBodyRotationType::NO_ROTATE) {
        rv.first->setAngularFactor(0.0);
    }
    else if (rotationType == RigidBodyRotationType::NO_ROTATE_XY) {
        rv.first->setAngularFactor(btVector3(0.0, 0.0, 1.0));
    }

    //  TODO: Scaling that isnt global...
    // If we are convex we can apply local scaling
 /*   btConvexShape* convexShape = dynamic_cast<btConvexShape*>(rigidBody);
    if (convexShape) {
        convexShape->setLocalScaling(f32v3ToBtVector3(scale));
    }*/
    return rv;
}

void PhysicsWorld::deleteRigidBody(btRigidBody* rigidBody) {
    mRigidBodiesToDelete.enqueue(rigidBody);
}

void PhysicsWorld::deleteStaticPhysicsMesh(StaticPhysicsMesh&& physicsMesh)
{
    mStaticPhysicsMeshesToDelete.enqueue(std::move(physicsMesh));
}

void PhysicsWorld::addStaticMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder, OUT StaticPhysicsMesh& outMesh) {
    PROFILE_FUNCTION();
    std::lock_guard lock(mMutex);
    outMesh.mPhysicsMesh = std::make_unique<btTriangleIndexVertexArray>();
    // Cache the vertex and index data because bullet uses our memory rather than a copy
    // TODO: Compress this? Because its a vector it may have extra capacity
    outMesh.mVerts = std::move(meshBuilder.mVerts);
    outMesh.mIndices = std::move(meshBuilder.mIndices);

    btIndexedMesh indexedMesh;
    indexedMesh.m_vertexType = PHY_FLOAT;
    indexedMesh.m_vertexStride = sizeof(f32v3);
    indexedMesh.m_numVertices = outMesh.mVerts.size();
    indexedMesh.m_vertexBase = (unsigned char*)outMesh.mVerts.data();
    indexedMesh.m_triangleIndexBase = (unsigned char*)outMesh.mIndices.data();
    indexedMesh.m_triangleIndexStride = 3 * sizeof(ui32);
    indexedMesh.m_numTriangles = outMesh.mIndices.size() / 3;
    outMesh.mPhysicsMesh->addIndexedMesh(indexedMesh, PHY_ScalarType::PHY_INTEGER);

    assert(!outMesh.mRigidBody);
    btTransform startTransform;
    startTransform.setOrigin(f32v3ToBtVector3(meshBuilder.getRootPos()));
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    // TODO: House owner entity
    outMesh.mShape = std::make_unique<btBvhTriangleMeshShape>(outMesh.mPhysicsMesh.get(), true /*aabbCompression*/);
    outMesh.mRigidBody = createRigidBody(entt::null, 0.0f, startTransform, outMesh.mShape.get()).first;
}

RigidBodyPair PhysicsWorld::createRigidBody(entt::entity ownerEntity, btScalar mass, const btTransform& startTransform, btCollisionShape* shape)
{
    PROFILE_FUNCTION();
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

    if (ownerEntity == entt::null) {
        body->setUserIndex(INT32_MAX);
    }
    else {
        assert((size_t)ownerEntity <= INT32_MAX && "Entity ID overflow in createRigidBody");
        body->setUserIndex((int)ownerEntity); // TODO: ENTT?
    }

    mDynamicsWorld->addRigidBody(body);
    //mRigidBodiesToAdd.enqueue(body);

    RigidBodyPair rv;
    rv.first = body;
    // Get shape half height
    switch (shape->getShapeType()) {
        case BroadphaseNativeTypes::CAPSULE_SHAPE_PROXYTYPE:
            rv.second = ((btCapsuleShape*)shape)->getHalfHeight() + ((btCapsuleShape*)shape)->getRadius();
            break;
        case BroadphaseNativeTypes::TRIANGLE_MESH_SHAPE_PROXYTYPE:
        case BroadphaseNativeTypes::TERRAIN_SHAPE_PROXYTYPE:
            rv.second = 0.0f;
            break;
        default:
            rv.second = 0.0f;
            pError("Need to implement new shape type!");
            assert(false && "Need to implement new shape type!");
    }
    return rv;
}

void PhysicsWorld::debugRender() const {
    assert(IS_RENDER_THREAD());

    const bool showStatic = sDebugOptions.mShowStaticPhysics;
    const bool showDynamic = sDebugOptions.mShowDynamicPhysics;
    const bool showTerrain = sDebugOptions.mShowTerrainPhysics;
    bool renderStaticPass = false;

    // Refresh static geometry
    if (mWasRenderingStatic != showStatic || mWasRenderingTerrain != showTerrain) {
        mWasRenderingStatic = showStatic;
        mWasRenderingTerrain = showTerrain;
        mDebugDrawer->clearStaticLines();
        renderStaticPass = true;
    }

    if (renderStaticPass) {
        // Draw static and dynamic
        ScopedTimer timer("Static debug");
        // TODO: We might still have race condition with adding rigidbodies
        std::shared_lock lock(mMutex);
        if (showTerrain) mDebugDrawer->reserveStaticLines(2000000);

        for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body->getMass()) {
                if (showDynamic) {
                    mDebugDrawer->setIsStaticMode(false);
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
                }
            }
            else {
                // Terrain and other static geo
                if (body->getCollisionShape()->getShapeType() == BroadphaseNativeTypes::TERRAIN_SHAPE_PROXYTYPE) {
                    if (showTerrain) {
                        mDebugDrawer->setIsStaticMode(true);
                        mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_TERRAIN);
                    }
                }
                else if (showStatic) {
                    mDebugDrawer->setIsStaticMode(true);
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_STATIC);
                }
            }
        }
    }
    else if (showDynamic) {
        std::shared_lock lock(mMutex);
        for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body->getMass()) {
                mDebugDrawer->setIsStaticMode(false);
                mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
            }
        }
    }

    if (sDebugOptions.mShowPhysicsActions) {
        std::lock_guard lock(mMutex);
        // Draw actions
        const auto& actions = mDynamicsWorld->getActions();
        for (int i = 0; i < actions.size(); ++i) {
            actions[i]->debugDraw(mDebugDrawer.get());
        }
    }
    
}

struct CustomRayResult : public btCollisionWorld::ClosestRayResultCallback
{
    CustomRayResult(const btVector3& rayFromWorld, const btVector3& rayToWorld)
        : btCollisionWorld::ClosestRayResultCallback(rayFromWorld, rayToWorld) // TODO: UNUSED?
    {
    }

    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& r, bool b)
    {
        if (b && r.m_hitFraction < m_closestHitFraction) {
            mHitNormal = btVector3ToF32v3(r.m_hitNormalLocal);
            m_closestHitFraction = r.m_hitFraction;
            m_collisionObject = r.m_collisionObject;
            //r.m_localShapeInfo
        }
        return r.m_hitFraction;
    }

    f32v3 mHitNormal = f32v3(0.0f);
};

PhysHitResult PhysicsWorld::pick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes) const
{
    btVector3 start = f32v3ToBtVector3(rayStart);
    btVector3 end = f32v3ToBtVector3(rayEnd);
    // TODO: Use more of btCollisionWorld::ClosestRayResultCallback?
    CustomRayResult rayResult(start, end);
    int collisionMask = btBroadphaseProxy::DefaultFilter;

    if (pickTypes & PICK_TYPE_DYNAMIC) {
        collisionMask |= btBroadphaseProxy::KinematicFilter;
    }
    if (pickTypes & PICK_TYPE_STATIC) {
        collisionMask |= btBroadphaseProxy::StaticFilter;
    }
    rayResult.m_collisionFilterMask = collisionMask;
    //rayResult.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
    if (IS_GAME_THREAD()) {
        mDynamicsWorld->rayTest(start, end, rayResult);
    }
    else {
        std::shared_lock guard(mMutex);
        mDynamicsWorld->rayTest(start, end, rayResult);
    }

    PhysHitResult rv;
    rv.mTime = rayResult.m_closestHitFraction;
    rv.mNormal = rayResult.mHitNormal;
    rv.mCollisionObject = rayResult.m_collisionObject;
    rv.mPosition = rayStart + (rayEnd - rayStart) * rv.mTime;
    return rv;
}

bool PhysicsWorld::tryPick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes, OUT PhysHitResult& result) const {
    assert(!IS_GAME_THREAD());
    btVector3 start = f32v3ToBtVector3(rayStart);
    btVector3 end = f32v3ToBtVector3(rayEnd);
    // TODO: Use more of btCollisionWorld::ClosestRayResultCallback?
    CustomRayResult rayResult(start, end);
    int collisionMask = btBroadphaseProxy::DefaultFilter;

    if (pickTypes & PICK_TYPE_DYNAMIC) {
        collisionMask |= btBroadphaseProxy::KinematicFilter;
    }
    if (pickTypes & PICK_TYPE_STATIC) {
        collisionMask |= btBroadphaseProxy::StaticFilter;
    }
    rayResult.m_collisionFilterMask = collisionMask;
    //rayResult.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
    if (mMutex.try_lock_shared()) {
        mDynamicsWorld->rayTest(start, end, rayResult);
        mMutex.unlock_shared();
    } else {
        return false;
    }

    result.mTime = rayResult.m_closestHitFraction;
    result.mNormal = rayResult.mHitNormal;
    result.mCollisionObject = rayResult.m_collisionObject;
    result.mPosition = rayStart + (rayEnd - rayStart) * result.mTime;
    return true;
}
