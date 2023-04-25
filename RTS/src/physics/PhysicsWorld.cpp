#include "stdafx.h"
#include "PhysicsWorld.h"

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
//#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
//#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/BroadphaseCollision/btAxisSweep3.h"
#include "resources/TileRepository.h"

#include "world/IWorld.h"
#include "tile/TileContainer.h"

#include "debugging/PhysicsDebugDrawer.h"

#include "physics/DynamicCharacterController.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "terrain/HeightmapPatch.h"
#include "options/DebugOptions.h"

#include "debugging/DebugRenderer.h"
#include "physics/PhysicsConst.h"

// For custom physics tests
#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/PhysicsComponent.h"

#include "util/b3ChromeTraceUtil.h"

const btVector3 DEBUG_COLOR_DYNAMIC(0.0, 1.0, 0.0);
const btVector3 DEBUG_COLOR_STATIC(1.0, 0.0, 0.0);
const btVector3 DEBUG_COLOR_TERRAIN(1.0, 1.0, 1.0);

constexpr int COLLISION_FILTER_ALL = 0xffffffff;
constexpr int COLLISION_FILTER_DYNAMIC = BIT_CAST(CollisionGroup::QUERY) | BIT_CAST(CollisionGroup::CHARACTER);
constexpr int COLLISION_FILTER_STATIC = BIT_CAST(CollisionGroup::QUERY) | BIT_CAST(CollisionGroup::TERRAIN) | BIT_CAST(CollisionGroup::STATIC);

int collisionMasks[e_cast(CollisionGroup::COUNT)] = {
    COLLISION_FILTER_ALL, // Query
    BIT_CAST(CollisionGroup::QUERY), // Terrain
    BIT_CAST(CollisionGroup::STATIC) | COLLISION_FILTER_DYNAMIC, // Character
    COLLISION_FILTER_DYNAMIC // Static
};

static_assert(e_cast(CollisionGroup::COUNT) == 4);

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
    mDynamicsWorld->setForceUpdateAllAabbs(false); // Attempt to boost performance

    constexpr size_t reserveColliderCount = 32768;
    mFreeStaticCollisionObjects.resize(reserveColliderCount);
    // Preallocate a bunch of objects so we dont have to later
    for (auto&& obj : mFreeStaticCollisionObjects) {
        obj = new btCollisionObject();
    }

    initEventHandlers();
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

    for (auto&& f : mFreeStaticCollisionObjects) {
        delete f;
    }

    //delete collision shapes
   /* for (int j = 0; j < m_collisionShapes.size(); j++)
    {
        btCollisionShape* shape = m_collisionShapes[j];
        delete shape;
    }
    m_collisionShapes.clear();*/
}

int PhysicsWorld::stepSimulation(f32 elapsedSec) {
    assert(IS_GAME_THREAD());
    PROFILE_FUNCTION();

    int stepCount = 0;
    { // Simulate
        PROFILE_SCOPE("Step");
        std::lock_guard guard(mStepSimulationMutex);
        stepCount = mDynamicsWorld->stepSimulation(elapsedSec, 3 /*maxSubSteps*/);
    }

    // Process picking from other threads
    constexpr int BULK_DEQUEUE_SIZE = 64;
    std::pair<PickParams, DeferredPhysicsPick*> pickBuffer[BULK_DEQUEUE_SIZE];
    if (size_t count = mDeferredPicks.try_dequeue_bulk(pickBuffer, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            const PickParams& params = pickBuffer[i].first;
            DeferredPhysicsPick* deferredPick = pickBuffer[i].second;
            PhysHitResult result = pick(params.rayStart, params.rayEnd, params.pickTypes, deferredPick->getQueryFlags());
            deferredPick->setPickResult(result);
        }
    }

    return stepCount;
}

DynamicCharacterController* PhysicsWorld::addDynamicCharacterController(entt::entity ownerEntity, btRigidBody* rigidBody, f32 rotationYaw) {
    assert(IS_GAME_THREAD());
    assert(rigidBody->getCollisionShape()->getShapeType() == BroadphaseNativeTypes::CAPSULE_SHAPE_PROXYTYPE);
    DynamicCharacterController* dynamicCharacterController = new DynamicCharacterController(rigidBody, (btCapsuleShape*)rigidBody->getCollisionShape());
    mDynamicsWorld->addAction(dynamicCharacterController);
    return dynamicCharacterController;
}

btCollisionObject* PhysicsWorld::addHeightField(const HeightmapPatch& patch)
{
    assert(IS_GAME_THREAD());
    btTransform startTransform;
    const f32v3 center = patch.mHeightData->aabb.getCenter();

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

    return createStaticCollisionObject(INVALID_PHYSICS_USER_INDEX, INVALID_PHYSICS_USER_INDEX, center, heightFieldShape, CollisionGroup::TERRAIN);
}

void PhysicsWorld::deleteHeightField(HeightmapPatch& patch) {
    assert(IS_GAME_THREAD());
    auto&& it = mHeightShapes.find(patch.mHeightData);
    assert(it != mHeightShapes.end());
    delete it->second;
    mHeightShapes.erase(it);
    mDynamicsWorld->removeCollisionObject(patch.mHeightData->mCollider);
    freeStaticCollisionObject(patch.mHeightData->mCollider);
    patch.mHeightData->mCollider = nullptr;
}

RigidBodyPair PhysicsWorld::addRigidBody(entt::entity ownerEntity, const f32v3& position, CollisionShapes shapeType, const f32v3& halfExtents, f32 mass, CollisionGroup group, RigidBodyRotationType rotationType /*= RigidBodyRotationType::FULL*/) {
    CollisionShapeID shapeId = mShapeRepository.getOrAddCollisionShape(shapeType, halfExtents);
    return addRigidBody(ownerEntity, position, mShapeRepository.getShape(shapeId), mass, group, rotationType);
}

RigidBodyPair PhysicsWorld::addRigidBody(entt::entity ownerEntity, const f32v3& position, btCollisionShape* collisionShape, f32 mass, CollisionGroup group, RigidBodyRotationType rotationType /*= RigidBodyRotationType::FULL*/) {
    assert((BIT_CAST(group) & COLLISION_FILTER_STATIC) == 0);
    RigidBodyPair rv = createRigidBody(ownerEntity, mass, position, collisionShape, group);

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


void PhysicsWorld::addTrackedStaticCollisionObjectAtPosition(TileContainerID containerOwner, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* collisionShape) {
    SpatialCollisionObjectLookup& lookup = mTileContainerPhysicsData[containerOwner].mSpatialCollisionObjectLookup;
#ifdef DEBUG
    const auto&& it = lookup.find(ownerTilePosition);
    assert(it == lookup.end());
#endif
    lookup[ownerTilePosition] = createStaticCollisionObject(containerOwner, ownerTilePosition, position, collisionShape, CollisionGroup::STATIC);
}


void PhysicsWorld::removeTrackedStaticCollisionObjectAtPosition(TileContainerID containerId, TileIndex tileIndex) {
    auto&& it = mTileContainerPhysicsData.find(containerId);
    if (it != mTileContainerPhysicsData.end()) {
        SpatialCollisionObjectLookup& lookup = it->second.mSpatialCollisionObjectLookup;
        auto&& spit = lookup.find(tileIndex);
        if (spit != lookup.end()) {
            {
                std::lock_guard lock(mStepSimulationMutex);
                mDynamicsWorld->removeCollisionObject(spit->second);
            }
            freeStaticCollisionObject(spit->second);
            lookup.erase(spit);
        }
    }
}

void PhysicsWorld::deletePhysicsForTileContainer(TileContainerID container)
{
    auto&& it = mTileContainerPhysicsData.find(container);
    if (it != mTileContainerPhysicsData.end()) {
        deleteStaticPhysicsMesh(std::move(it->second.mStaticMesh));
        for (auto& collisionObjectPair : it->second.mSpatialCollisionObjectLookup) {
            mDynamicsWorld->removeCollisionObject(collisionObjectPair.second);
            freeStaticCollisionObject(collisionObjectPair.second);
        }
        mTileContainerPhysicsData.erase(it);
    }
}

void PhysicsWorld::deleteRigidBody(btRigidBody* rigidBody) {
    assert(IS_GAME_THREAD());
    --mNumDynamicCollisionObjects;
    mDynamicsWorld->removeRigidBody(rigidBody);
    delete rigidBody;
}

void PhysicsWorld::deleteStaticPhysicsMesh(StaticPhysicsMesh&& physicsMesh) {
    if (physicsMesh.isValid()) {
        --mNumStaticCollisionObjects;
        mStaticPhysicsMeshesToDelete.enqueue(std::move(physicsMesh));
    }
}

void PhysicsWorld::addStaticMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder) {
    PROFILE_FUNCTION();
    assert(IS_GAME_THREAD());
    const TileContainerID tileContainerId = meshBuilder.mTrackedRigidBodyGatherer.mContainerId;
    assert(meshBuilder.hasAnyCollision());

    TileContainerPhysicsData* physicsData;
    deletePhysicsForTileContainer(meshBuilder.getOwnerTileContainerID());
    {
        std::lock_guard lock(mStepSimulationMutex);
        physicsData = &mTileContainerPhysicsData[tileContainerId];
    }
    StaticPhysicsMesh& staticMesh = physicsData->mStaticMesh;
    // Cache the vertex and index data because bullet uses our memory rather than a copy
    // TODO: Compress this? Because its a vector it may have extra capacity
    staticMesh.mVerts = std::move(meshBuilder.mVerts);
    staticMesh.mIndices = std::move(meshBuilder.mIndices);

    if (staticMesh.mVerts.size()) {
        staticMesh.mPhysicsMesh = std::make_unique<btTriangleIndexVertexArray>();
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
        staticMesh.mCollisionObject = createStaticCollisionObject(tileContainerId, INVALID_PHYSICS_USER_INDEX, meshBuilder.getRootPos(), staticMesh.mShape.get(), CollisionGroup::STATIC);
    }

    // Add all tracked bodies
    addTrackedStaticRigidBodiesFromGatherer(meshBuilder.mTrackedRigidBodyGatherer, physicsData->mSpatialCollisionObjectLookup);
}

void PhysicsWorld::initEventHandlers() {
    TileContainerRepository::registerTileContainerListeners(mTileContainerEventListeners);
    // TODO: Profile version without lambda capture?
    TileContainerRepository::addDestroyListener(mTileContainerEventListeners, [this](const TileContainerEvent& event) {
        assert(IS_GAME_THREAD());
        deletePhysicsForTileContainer(event.container->getId());
    });
    TileContainerRepository::addEditTilesListener(mTileContainerEventListeners, [this](const TileContainerEvent& event) {
        assert(IS_GAME_THREAD());
        const TileContainerEditEvent& editEvent = event.edit;
        switch (event.edit.type) {
            case TileContainerEditEventType::ChangeFlags:
                break;
            case TileContainerEditEventType::ChangeLayer: {
                for (ui32 i = 0; i < editEvent.editCount; ++i) {
                    TileContainerEditLayerEventData& edit = editEvent.changeLayerArray[i];
                    const TileID prevId = edit.prevId;
                    if (prevId != TILE_ID_NONE) {
                        const TileData& prevTileData = TileRepository::getTileData(edit.prevId);
                        if (prevTileData.collisionShapeID != INVALID_COLLISION_SHAPE_ID) {
                            removeTrackedStaticCollisionObjectAtPosition(event.container->getId(), edit.tileIndex);
                        }
                    }
                    const TileID newId = edit.newId;
                    if (newId != TILE_ID_NONE) {
                        const TileData& tileData = TileRepository::getTileData(newId);
                        if (tileData.collisionShapeID != INVALID_COLLISION_SHAPE_ID) {
                            addTrackedStaticCollisionObjectAtPosition(event.container->getId(), edit.tileIndex, edit.worldPosition, mShapeRepository.getShape(tileData.collisionShapeID));
                        }
                    }
                }
                break;
            }
            case TileContainerEditEventType::ChangeZPos:
                break;
            case TileContainerEditEventType::ChangeOrientation:
                break;
            case TileContainerEditEventType::ChangeWall:
                break;
            default:
                assert(false && "Unhandled model edit event in InstancedStaticModelRenderer");
                break;
        }
        static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");
    });
}

void PhysicsWorld::addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, SpatialCollisionObjectLookup& lookup) {
    assert(IS_GAME_THREAD());
    PROFILE_FUNCTION();
    if (gatherer.mRigidBodiesToAdd.empty()) {
        return;
    }

    for (auto& it : gatherer.mRigidBodiesToAdd) {
#ifdef DEBUG
        //const auto&& it2 = lookup.find(it.position);
        //assert(it2 == lookup.end());
#endif
        lookup[it.ownerTilePosition] = createStaticCollisionObject(gatherer.mContainerId, it.ownerTilePosition, it.position, mShapeRepository.getShape(it.shapeId), CollisionGroup::STATIC);
    }
}


RigidBodyPair PhysicsWorld::createRigidBody(entt::entity ownerEntity, btScalar mass, const f32v3& position, btCollisionShape* shape, CollisionGroup group)
{
    PROFILE_FUNCTION();
    btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));
    btTransform startTransform;
    const f32 halfShapeHeight = getShapeHalfHeight(shape);
    startTransform.setOrigin(btVector3(position.x, position.y, position.z + halfShapeHeight)); // Offset upward so we dont spawn in the ground
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));

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
    body->setGravity(GRAVITY);

    assert(IS_GAME_THREAD());
    {
        std::lock_guard lock(mStepSimulationMutex);
        mDynamicsWorld->addRigidBody(body, BIT_CAST(group), collisionMasks[e_cast(group)]);
    }
    //mRigidBodiesToAdd.enqueue(body);

    RigidBodyPair rv;
    rv.first = body;
    rv.second = halfShapeHeight;

    ++mNumDynamicCollisionObjects;
    return rv;
}

btCollisionObject* PhysicsWorld::createStaticCollisionObject(TileContainerID ownerTileContainer, TileIndex ownerTilePosition, const f32v3& position, btCollisionShape* shape, CollisionGroup group) {
    PROFILE_FUNCTION();
    btTransform startTransform;
    const f32 halfHeight = getShapeHalfHeight(shape);
    startTransform.setOrigin(btVector3(position.x, position.y, position.z + halfHeight)); // TODO: not always offset up?
    startTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

    btVector3 localInertia(0, 0, 0);
    btCollisionObject* object = allocStaticCollisionObject();
    object->setCollisionShape(shape);
    object->setWorldTransform(startTransform);

    // Only entities use user index 0
    object->setUserIndex(INVALID_PHYSICS_USER_INDEX);

    assert(ownerTileContainer <= INVALID_PHYSICS_USER_INDEX && "Tile container ID overflow in createRigidBody");
    object->setUserIndex2(ownerTileContainer);

    assert(ownerTilePosition <= INVALID_PHYSICS_USER_INDEX && "Tile index overflow in createRigidBody");
    object->setUserIndex3(ownerTilePosition);

    assert(IS_GAME_THREAD());
    mDynamicsWorld->addCollisionObject(object, BIT_CAST(group), collisionMasks[e_cast(group)]);
    object->setActivationState(DISABLE_SIMULATION);
   
    ++mNumStaticCollisionObjects;
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

btCollisionObject* PhysicsWorld::allocStaticCollisionObject() {
    if (mFreeStaticCollisionObjects.size()) {
        btCollisionObject* obj = mFreeStaticCollisionObjects.back();
        *obj = btCollisionObject();
        mFreeStaticCollisionObjects.pop_back();
        return obj;
    }
    return new btCollisionObject();
}

void PhysicsWorld::freeStaticCollisionObject(btCollisionObject* obj) {
    mFreeStaticCollisionObjects.push_back(obj);
}

void PhysicsWorld::debugRender() const {
    assert(IS_RENDER_THREAD());
    PROFILE_FUNCTION();

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
        if (showTerrain) mDebugDrawer->reserveStaticLines(2000000);

        for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMass()) {
                if (showDynamic) {
                    mDebugDrawer->setIsStaticMode(false);
                    mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
                }
            }
            else {
                // Terrain and other static geo
                if (obj->getCollisionShape()->getShapeType() == BroadphaseNativeTypes::TERRAIN_SHAPE_PROXYTYPE) {
                    if (showTerrain) {
                        mDebugDrawer->setIsStaticMode(true);
                        mDynamicsWorld->debugDrawObject(obj->getWorldTransform(), obj->getCollisionShape(), DEBUG_COLOR_TERRAIN);
                    }
                }
                else if (showStatic) {
                    mDebugDrawer->setIsStaticMode(true);
                    mDynamicsWorld->debugDrawObject(obj->getWorldTransform(), obj->getCollisionShape(), DEBUG_COLOR_STATIC);
                }
            }
        }
    }
    else if (showDynamic) {
        for (int i = mDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = mDynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMass()) {
                mDebugDrawer->setIsStaticMode(false);
                mDynamicsWorld->debugDrawObject(body->getWorldTransform(), body->getCollisionShape(), DEBUG_COLOR_DYNAMIC);
            }
        }
    }

    if (sDebugOptions.mShowPhysicsActions) {
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

PhysHitResult PhysicsWorld::pick(const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes, BitFlags<PhysicsPickQueryFlags> queryFlags) const
{
    assert(IS_GAME_THREAD());
    PROFILE_FUNCTION();
    btVector3 start = f32v3ToBtVector3(rayStart);
    btVector3 end = f32v3ToBtVector3(rayEnd);
    // TODO: Use more of btCollisionWorld::ClosestRayResultCallback?
    CustomRayResult rayResult(start, end);
    int collisionMask = 0;

    if (pickTypes & PICK_TYPE_DYNAMIC) {
        collisionMask |= COLLISION_FILTER_DYNAMIC;
    }
    if (pickTypes & PICK_TYPE_STATIC) {
        collisionMask |= COLLISION_FILTER_STATIC;
    }
    rayResult.m_collisionFilterGroup = BIT_CAST(CollisionGroup::QUERY);
    rayResult.m_collisionFilterMask = collisionMask;
    //rayResult.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
    mDynamicsWorld->rayTest(start, end, rayResult);

    PhysHitResult rv;
    rv.mTime = rayResult.m_closestHitFraction;
    rv.mNormal = rayResult.mHitNormal;
    rv.mCollisionObject = rayResult.m_collisionObject;
    rv.mPosition = rayStart + (rayEnd - rayStart) * rv.mTime;
    if (queryFlags.isBitSet(PhysicsPickQueryFlags::QUERY_TILE_INFO) && rv.mCollisionObject) {
        
        if (rv.mCollisionObject->getUserIndex() != INVALID_PHYSICS_USER_INDEX) {
            rv.mSelectedEntity = entt::entity(rv.mCollisionObject->getUserIndex());
        }
        else {
            TileContainerID containerId = rv.mCollisionObject->getUserIndex2();
            if (containerId != INVALID_PHYSICS_USER_INDEX) {
                rv.mContainerID = containerId;
                TileIndex index = rv.mCollisionObject->getUserIndex3();
                if (index != INVALID_PHYSICS_USER_INDEX) {
                    rv.mTileIndex = index;
                }
                else {
                    f32v2 tilePos2D(rv.mPosition.x, rv.mPosition.y);
                    TileHandle handle = sMainGameWorld->getTerrainTileHandleAtWorldPos(tilePos2D);
                    if (handle.isValid()) {
                        assert(tilePos2D.x >= 0.0f && tilePos2D.y >= 0.0f);
                        std::vector<Structure*> structures = sMainGameWorld->tryGetStructuresAtWorldPos(i32v2(tilePos2D));
                        for (auto&& structure : structures) {
                            TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(rv.mPosition);
                            if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                                rv.mTileIndex = nextHandle.tileIndex;
                                break;
                            }
                        }
                        /*Chunk* chunk = handle.container->getOwnerChunk();
                        if (chunk->isDataReady()) {
                            StructureArrayPtr structures = chunk->getStructuresAt(handle.tileIndex);
                            for (int i = 0; i < structures.second; ++i) {
                                Structure* structure = structures.first[i];
                                TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(rv.mPosition);
                                if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                                    rv.mTileIndex = nextHandle.tileIndex;
                                    break;
                                }
                            }
                        }*/
                    }
                    else {
                        // Need to query which tile we selected
                        LOG_DEBUG("Selected invalid chunk in ray pick");
                    }
                }
            }
        }
    }

    return rv;
}

void PhysicsWorld::pickDeferred(DeferredPhysicsPick* deferredPick, const f32v3& rayStart, const f32v3& rayEnd, PickTypes pickTypes, BitFlags<PhysicsPickQueryFlags> queryFlags) {
    deferredPick->setQueryFlags(queryFlags);
    mDeferredPicks.enqueue(std::pair<PickParams, DeferredPhysicsPick*>(PickParams{rayStart, rayEnd, pickTypes}, deferredPick));
}

void PhysicsWorld::startB3Profiling() {
    {
        std::lock_guard guard(mStepSimulationMutex);
        b3ChromeUtilsStartTimings();
    }
    LOG_DEBUG("Begin bullet profiling");
    mIsProfiling = true;
}

void PhysicsWorld::endB3ProfilingAndDumpToFile(const char* fileNamePrefix) {
    {
        std::lock_guard guard(mStepSimulationMutex);
        if (mIsProfiling) {
            b3ChromeUtilsStopTimingsAndWriteJsonFile(fileNamePrefix);
        }
    }

    LOG_DEBUG("End bullet profiling, dumping chrome trace file");
    mIsProfiling = false;
}
