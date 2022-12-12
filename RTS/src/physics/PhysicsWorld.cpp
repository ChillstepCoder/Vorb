#include "stdafx.h"
#include "PhysicsWorld.h"

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
//#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
//#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/BroadphaseCollision/btAxisSweep3.h"

#include "world/IWorld.h"
#include "tile/TileContainer.h"

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


TileContainerEventDispatcher::Handle sTileContainerDestroyHandle;

PhysicsWorld::PhysicsWorld(CollisionShapeRepository& shapeRepository) : mShapeRepository(shapeRepository) {
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

    // Listen for tile containers to be destroyed
    assert(!sTileContainerDestroyHandle);
    sTileContainerDestroyHandle = TileContainerRepository::addDestroyListener([](const TileContainer& container) {
        assert(IS_GAME_THREAD());
        sWorld->getPhysicsWorld().deletePhysicsForTileContainer(container.getId());
    });
}


PhysicsWorld::~PhysicsWorld() {

    sTileContainerDestroyHandle.reset();

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
                mDynamicsWorld->removeCollisionObject(staticPhysicsMeshes[i].mCollisionObject);
                delete staticPhysicsMeshes[i].mCollisionObject;
            }
        }
    }

    // Process picking from other threads
    std::pair<PickParams, DeferredPhysicsPick*> pickBuffer[BULK_DEQUEUE_SIZE];
    if (size_t count = mDeferredPicks.try_dequeue_bulk(pickBuffer, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            const PickParams& params = pickBuffer[i].first;
            DeferredPhysicsPick* deferredPick = pickBuffer[i].second;
            PhysHitResult result = pick(params.rayStart, params.rayEnd, params.pickTypes);
            deferredPick->setPickResult(result);
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

RigidBodyPair PhysicsWorld::addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shapeType, const f32v3& halfExtents, f32 mass, RigidBodyRotationType rotationType /*= RigidBodyRotationType::FULL*/) {
    CollisionShapeID shapeId = mShapeRepository.getOrAddCollisionShape(shapeType, halfExtents);
    return addRigidBody(ownerEntity, position, mShapeRepository.getShape(shapeId), mass, rotationType);
}

RigidBodyPair PhysicsWorld::addRigidBody(entt::entity ownerEntity, const f32v3& position, btCollisionShape* collisionShape, f32 mass, RigidBodyRotationType rotationType /*= RigidBodyRotationType::FULL*/) {
    btTransform startTransform;
    startTransform.setOrigin(btVector3(position.x, position.y, position.z));
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));

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


void PhysicsWorld::addTrackedStaticRigidBodyAtPosition(TileContainerID containerOwner, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* collisionShape) {
    SpatialRigidBodyLookup& lookup = mTileContainerPhysicsData[containerOwner].mSpatialRigidBodyLookup;
#ifdef DEBUG
    const auto&& it = lookup.find(position);
    assert(it == lookup.end());
#endif
    lookup[position] = createStaticCollisionObject(containerOwner, ownerTilePosition, position, collisionShape);
}

void PhysicsWorld::removeTrackedStaticRigidBodyAtPosition(TileContainerID containerOwner, const f32v3& position) {
    assert(IS_GAME_THREAD());
    auto&& it = mTileContainerPhysicsData.find(containerOwner);
    assert(it != mTileContainerPhysicsData.end());

    SpatialRigidBodyLookup& lookup = it->second.mSpatialRigidBodyLookup;
    auto&& it2 = lookup.find(position);
    assert(it2 != lookup.end());

    mDynamicsWorld->removeCollisionObject(it2->second);
    delete it2->second;
    lookup.erase(it2);
}

void PhysicsWorld::deletePhysicsForTileContainer(TileContainerID container)
{
    auto&& it = mTileContainerPhysicsData.find(container);
    if (it != mTileContainerPhysicsData.end()) {
        deleteStaticPhysicsMesh(std::move(it->second.mStaticMesh));
        for (auto& collisionObjectPair : it->second.mSpatialRigidBodyLookup) {
            mDynamicsWorld->removeCollisionObject(collisionObjectPair.second);
            delete collisionObjectPair.second;
        }
        mTileContainerPhysicsData.erase(it);
    }
}

void PhysicsWorld::deleteRigidBody(btRigidBody* rigidBody) {
    mRigidBodiesToDelete.enqueue(rigidBody);
}

void PhysicsWorld::deleteStaticPhysicsMesh(StaticPhysicsMesh&& physicsMesh) {
    if (physicsMesh.isValid()) {
        mStaticPhysicsMeshesToDelete.enqueue(std::move(physicsMesh));
    }
}

void PhysicsWorld::addStaticMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder) {
    PROFILE_FUNCTION();
    assert(IS_GAME_THREAD());
    const TileContainerID tileContainerId = meshBuilder.mTrackedRigidBodyGatherer.mContainerId;

    std::lock_guard lock(mMutex);
    TileContainerPhysicsData& physicsData = mTileContainerPhysicsData[tileContainerId];
    StaticPhysicsMesh& staticMesh = physicsData.mStaticMesh;
    assert(!staticMesh.isValid());
    staticMesh.mPhysicsMesh = std::make_unique<btTriangleIndexVertexArray>();
    // Cache the vertex and index data because bullet uses our memory rather than a copy
    // TODO: Compress this? Because its a vector it may have extra capacity
    staticMesh.mVerts = std::move(meshBuilder.mVerts);
    staticMesh.mIndices = std::move(meshBuilder.mIndices);

    if (staticMesh.mVerts.size()) {
        btIndexedMesh indexedMesh;
        indexedMesh.m_vertexType = PHY_FLOAT;
        indexedMesh.m_vertexStride = sizeof(f32v3);
        indexedMesh.m_numVertices = staticMesh.mVerts.size();
        indexedMesh.m_vertexBase = (unsigned char*)staticMesh.mVerts.data();
        indexedMesh.m_triangleIndexBase = (unsigned char*)staticMesh.mIndices.data();
        indexedMesh.m_triangleIndexStride = 3 * sizeof(ui32);
        indexedMesh.m_numTriangles = staticMesh.mIndices.size() / 3;
        staticMesh.mPhysicsMesh->addIndexedMesh(indexedMesh, PHY_ScalarType::PHY_INTEGER);

        assert(!staticMesh.mCollisionObject);
        btTransform startTransform;
        startTransform.setOrigin(f32v3ToBtVector3(meshBuilder.getRootPos()));
        startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
        // TODO: House owner entity
        staticMesh.mShape = std::make_unique<btBvhTriangleMeshShape>(staticMesh.mPhysicsMesh.get(), true /*aabbCompression*/);
        staticMesh.mCollisionObject = createStaticCollisionObject(tileContainerId, INT32_MAX, meshBuilder.getRootPos(), staticMesh.mShape.get());
    }

    // Add all tracked bodies
    addTrackedStaticRigidBodiesFromGatherer(meshBuilder.mTrackedRigidBodyGatherer, physicsData.mSpatialRigidBodyLookup);
}


void PhysicsWorld::addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, SpatialRigidBodyLookup& lookup) {
    assert(IS_GAME_THREAD());
    if (gatherer.mRigidBodiesToAdd.empty()) {
        return;
    }

    for (auto& it : gatherer.mRigidBodiesToAdd) {
#ifdef DEBUG
        const auto&& it2 = lookup.find(it.position);
        assert(it2 == lookup.end());
#endif
        lookup[it.position] = createStaticCollisionObject(gatherer.mContainerId, it.ownerTilePosition, it.position, mShapeRepository.getShape(it.shapeId));
    }
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
        body->setUserIndex(INVALID_PHYSICS_USER_INDEX);
    }
    else {
        assert((size_t)ownerEntity <= INVALID_PHYSICS_USER_INDEX && "Entity ID overflow in createRigidBody");
        body->setUserIndex((int)ownerEntity); // TODO: ENTT?
    }

    assert(IS_GAME_THREAD());
    mDynamicsWorld->addRigidBody(body);
    //mRigidBodiesToAdd.enqueue(body);

    RigidBodyPair rv;
    rv.first = body;
    rv.second = getShapeHalfHeight(shape);

    return rv;
}

btCollisionObject* PhysicsWorld::createStaticCollisionObject(TileContainerID ownerTileContainer, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* shape) {
    btTransform startTransform;
    const f32 halfHeight = getShapeHalfHeight(shape);
    startTransform.setOrigin(btVector3(position.x, position.y, position.z + halfHeight)); // TODO: not always offset up?
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    PROFILE_FUNCTION();
    btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

    btVector3 localInertia(0, 0, 0);
    btCollisionObject* object = new btRigidBody(0.0f, 0, shape, localInertia);
    object->setWorldTransform(startTransform);

    assert(ownerTileContainer < INVALID_PHYSICS_USER_INDEX && "Tile container ID overflow in createRigidBody");
    object->setUserIndex2(ownerTileContainer);

    assert(ownerTilePosition < INVALID_PHYSICS_USER_INDEX && "Tile index overflow in createRigidBody");
    object->setUserIndex3(ownerTilePosition);

    assert(IS_GAME_THREAD());
    mDynamicsWorld->addCollisionObject(object);
    //mRigidBodiesToAdd.enqueue(body);
   
    return object;
}

f32 PhysicsWorld::getShapeHalfHeight(btCollisionShape* shape) const {
    // Get shape half height
    switch (shape->getShapeType()) {
        case BroadphaseNativeTypes::CAPSULE_SHAPE_PROXYTYPE:
            return ((btCapsuleShape*)shape)->getHalfHeight() + ((btCapsuleShape*)shape)->getRadius();
        case BroadphaseNativeTypes::CYLINDER_SHAPE_PROXYTYPE:
            return ((btCylinderShape*)shape)->getHalfExtentsWithoutMargin().z();
        case BroadphaseNativeTypes::BOX_SHAPE_PROXYTYPE:
            return ((btBoxShape*)shape)->getHalfExtentsWithoutMargin().z();
        case BroadphaseNativeTypes::SPHERE_SHAPE_PROXYTYPE:
            return ((btSphereShape*)shape)->getRadius();
        case BroadphaseNativeTypes::TRIANGLE_MESH_SHAPE_PROXYTYPE:
        case BroadphaseNativeTypes::TERRAIN_SHAPE_PROXYTYPE:
            return 0.0f;
        default:
            pError("Need to implement new shape type!");
            assert(false && "Need to implement new shape type!");
    }
    return 0.0f;
}

void PhysicsWorld::debugRender() const {
    assert(IS_RENDER_THREAD());
    PROFILE_SCOPE();

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

void PhysicsWorld::pickDeferred(DeferredPhysicsPick* deferredPick, const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes) {
    mDeferredPicks.enqueue(std::pair<PickParams, DeferredPhysicsPick*>(PickParams{rayStart, rayEnd, pickTypes}, deferredPick));
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
