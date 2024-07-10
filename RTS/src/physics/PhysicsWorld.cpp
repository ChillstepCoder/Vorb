#include "stdafx.h"
#include "PhysicsWorld.h"

// Jolt Documentation
// https://jrouwe.github.io/JoltPhysics/index.html

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Broadphase/BroadphaseLayerInterfaceMask.h>
#include <Jolt/Physics/Collision/Broadphase/ObjectVsBroadphaseLayerFilterMask.h>

#include "physics/PhysicsDebugRenderer.h"
#include "physics/StaticPhysicsMeshBuilder.h"
#include "physics/CollisionShapeRepository.h"
#include "physics/PhysicsBodyFilters.h"
#include "physics/PhysicsJobSystem.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"

#include "terrain/HeightmapPatch.h"

#include "options/DebugOptions.h"

#include "debugging/DebugRenderer.h"

#include "resources/ModelRepository.h"

static const JPH::Quat ROTATE_ZUP = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f);


PhysicsWorld* sGamePhysicsWorld = nullptr;

constexpr const char* PHYSICS_STEP_PROFILE_NAME = "Physics Step";

// Shared query shapes
static std::unique_ptr<JPH::SphereShape> sUnitSphereShape;
static std::unique_ptr<JPH::CylinderShape> sUnitCylinderShape;

// TODOS:
// 1. Character and CharacterVirtual https://jrouwe.github.io/JoltPhysics/index.html#character-controllers
// 2. Custom heightfield collision shape (Involved...) https://jrouwe.github.io/JoltPhysics/index.html#creating-custom-shapes

// Write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.
using namespace JPH::literals;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    LOG_INFO("{}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS

static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine) {
    panic("PHYSICS ASSERT FAILED: {}:{}: ({}) {}", inFile, inLine, inExpression, (inMessage != nullptr ? inMessage : ""));

    return true;
};

#endif // JPH_ENABLE_ASSERTS

class PhysicsBodyActivationListener : public JPH::BodyActivationListener{
public:
    PhysicsBodyActivationListener(PhysicsWorld& physicsWorld) : mPhysicsWorld(physicsWorld) {}

    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, ui64 inBodyUserData) override {
        PhysicsBodyUserData userData(inBodyUserData);
        if (userData.getType() == PhysicsBodyUserDataType::ItemEntity) {
            std::lock_guard lock(mPhysicsWorld.mItemEntitiesMutex);
            mPhysicsWorld.mItemEntitiesMovedThisFrame.insert(userData.getEntity());
        }
    }

    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, ui64 inBodyUserData) override {
        PhysicsBodyUserData userData(inBodyUserData);
        if (userData.getType() == PhysicsBodyUserDataType::ItemEntity) {
            std::lock_guard lock(mPhysicsWorld.mItemEntitiesMutex);
            mPhysicsWorld.mItemEntitiesRestedThisFrame.insert(userData.getEntity());
        }
    }

private:
    PhysicsWorld& mPhysicsWorld;
};


// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterfaceMask {
public:
    BPLayerInterfaceImpl() : JPH::BroadPhaseLayerInterfaceMask(BroadPhaseLayers::COUNT) {
        // Object layer masks will select the broadphase layer that first fits the inGroupsToInclude and !inGroupsToExclude
        // Include groups can collide with this broadphase layer, even if they are excluded, hence why for example Movable is
        // included on static layer
        ConfigureLayer(BroadPhaseLayers::Static, 
            PhysicsObjectLayer::Static | PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem | PhysicsObjectLayer::Character, // Include
            PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem | PhysicsObjectLayer::QueryPhantom // Exclude
        );
        ConfigureLayer(BroadPhaseLayers::Dynamic,
            PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem | PhysicsObjectLayer::Character, // Include
            PhysicsObjectLayer::QueryPhantom // Exclude
        );
        ConfigureLayer(BroadPhaseLayers::QueryOnly,
            PhysicsObjectLayer::QueryPhantom, // Include
            0 // Exclude
        );
    }
    static_assert(PhysicsObjectLayer::COUNT == 5, "Update");

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Dynamic:	return "Dynamic";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Static:	return "Static";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::QueryOnly: return "Query Only";
            default:													JPH_ASSERT(false); return "INVALID";
        }
        static_assert(BroadPhaseLayers::COUNT == 3);
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
};

// Can be called from multiple threads
//class MyContactListener : public JPH::ContactListener {
//public:
//    // See: ContactListener
//    virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override  {
//        LOG_INFO("Contact validate callback");
//
//        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
//        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
//    }
//
//    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
//        LOG_INFO("Contact added callback");
//    }
//
//    virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
//        LOG_INFO("Contact persisted callback");
//    }
//
//    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
//        LOG_INFO("Contact removed callback");
//    }
//};

// Collects results and stores in stack array of length MAX_COUNT
class PhysicsQueryCollector : public JPH::TransformedShapeCollector {
public:
    PhysicsQueryCollector(std::span<PhysicsQueryResult> outResults, const JPH::BodyInterface& bodyInterface) : outResults(outResults), bodyInterface(bodyInterface) {}

    void AddHit(const JPH::TransformedShape& inResult) override {
        if (collectedShapes < outResults.size()) [[likely]] {
            PhysicsQueryResult& result = outResults[collectedShapes++];
            result.mCenterOfMassPosition = f32v3(inResult.mShapePositionCOM.GetX(), inResult.mShapePositionCOM.GetY(), inResult.mShapePositionCOM.GetZ());
            result.mPhysBody = inResult.mBodyID.GetIndexAndSequenceNumber();
            result.mBodyUserData = bodyInterface.GetUserData(inResult.mBodyID);
            result.mShape = inResult.mShape;
        }
    }

    i32 collectedShapes = 0;
    std::span<PhysicsQueryResult> outResults;
    const JPH::BodyInterface& bodyInterface;
};

class PhysicsHitCollector : public JPH::CollideShapeCollector {
public:
    PhysicsHitCollector(std::span<PhysHitResult> outResults, const JPH::BodyInterface& bodyInterface) : outResults(outResults), bodyInterface(bodyInterface) {}

    void AddHit(const JPH::CollideShapeResult& inResult) override {
        if (collectedHits < outResults.size()) [[likely]] {
            PhysHitResult& result = outResults[collectedHits++];
            result.mHitBody = inResult.mBodyID2.GetIndexAndSequenceNumber();
            result.mPosition = f32v3(inResult.mContactPointOn2.GetX(), inResult.mContactPointOn2.GetY(), inResult.mContactPointOn2.GetZ());
            result.mBodyUserData = bodyInterface.GetUserData(inResult.mBodyID2);
            result.mShape = bodyInterface.GetShape(inResult.mBodyID2);
            result.mNormal = f32v3(inResult.mPenetrationAxis.GetX(), inResult.mPenetrationAxis.GetY(), inResult.mPenetrationAxis.GetZ());
            result.mTime = 0.0f; // Signifies hit
            result.mPenetrationDepth = inResult.mPenetrationDepth;
        }
    }

    i32 collectedHits = 0;
    std::span<PhysHitResult> outResults;
    const JPH::BodyInterface& bodyInterface;
};

// Minor attempt at some PIMPL
class JPHPhysicsWorldContext {
public:
    JPHPhysicsWorldContext(PhysicsWorld& physicsWorld) : bodyActivationListener(physicsWorld), objectVsBroadphaseLayerFilter(broadPhaseLayerInterface) {};

    void init(ui32 maxBodies, ui32 numBodyMutexes, ui32 maxBodyPairs, ui32 maxContactConstraints) {
        physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints, broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);
        // Z up
        physicsSystem.SetGravity(JPH::Vec3(0.0f, 0.0f, -9.81f));

        physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    }

    JPH::BodyInterface& getBodyInterface() {
        return physicsSystem.GetBodyInterface();
    }

    JPH::BodyInterface& getBodyInterfaceNonLocking() {
        ASSERT_GAME_THREAD();
        return physicsSystem.GetBodyInterfaceNoLock();
    }

    const JPH::BodyLockInterface& getBodyLockInterface() {
        return physicsSystem.GetBodyLockInterface();
    }

    void update(float deltaTime, int numCollisionSteps) {
        static constexpr int OPTIMIZATION_THRESHOLD = 10;
        static constexpr float OPTIMIZATION_INTERVAL = 2.0f;
        bool shouldOptimize = false;

        // Check if we've reached the threshold of new bodies
        if (mNewBodiesWithoutBroadphaseOptimize >= OPTIMIZATION_THRESHOLD) {
            shouldOptimize = true;
        }

        // Check if enough time has passed since last optimization
        mTimeSinceLastOptimization += deltaTime;
        if (mTimeSinceLastOptimization >= OPTIMIZATION_INTERVAL) {
            shouldOptimize = true;
        }

        // Optimize if necessary
        if (shouldOptimize) {
            PROFILE_SCOPE("Optimize Broadphase");
            physicsSystem.OptimizeBroadPhase();
            mNewBodiesWithoutBroadphaseOptimize = 0;
            mTimeSinceLastOptimization = 0.0f;
        }

        physicsSystem.Update(deltaTime, numCollisionSteps, &tempAllocator, &jobSystem);
    }

#ifdef JPH_DEBUG_RENDERER
    void debugDraw(const Camera3D& camera) {
        if (sPhysicsDebugRenderer->mRenderSettings.enableDebugDraw) {

            // TODO: Move into the debug renderer
            sPhysicsDebugRenderer->PrepareFrame(camera);
            // Only update static when static changes
            if (mDirtyStaticDebugRender) {
                StaticBodyDrawFilter staticFilter;
                sPhysicsDebugRenderer->PreDraw(true);
                physicsSystem.DrawBodies(sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings, sPhysicsDebugRenderer.get(), &staticFilter);
                mDirtyStaticDebugRender = false;
            }
            // Always update dynamic
            DynamicBodyDrawFilter dynamicFilter;
            sPhysicsDebugRenderer->PreDraw(false);
            physicsSystem.DrawBodies(sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings, sPhysicsDebugRenderer.get(), &dynamicFilter);
            sPhysicsDebugRenderer->EndFrame();
        }
    }

    bool mDirtyStaticDebugRender = true;
#endif

    void updateShape(PhysBodyID id, const JPH::Shape* newShape, bool updateMass, JPH::EActivation activateMode) {
        ASSERT_GAME_THREAD();
        JPH::BodyInterface& bodyInterface = getBodyInterfaceNonLocking();

        // TODO: Wake nearby bodies?
        bodyInterface.SetShape(JPH::BodyID(id), newShape, updateMass, activateMode);
#if ENABLE_PHYSICS_ANALYTICS == 1
        if (bodyInterface.GetMotionType(JPH::BodyID(id)) == JPH::EMotionType::Static) {
            mDirtyStaticDebugRender = true;
        }
#endif
    }

    JPH::Body& createBody(const JPH::BodyCreationSettings& createSettings, JPH::EActivation inActivationMode, CollisionShapeID shapeId) {
        ASSERT_GAME_THREAD();
        JPH::BodyInterface& bodyInterface = getBodyInterfaceNonLocking();
        JPH::Body* body = bodyInterface.CreateBody(createSettings);
        if (body == nullptr) {
            panic("Failed to create entity body with shape ID {}", (int)shapeId);
        }
        bodyInterface.AddBody(body->GetID(), inActivationMode);
        ++mNewBodiesWithoutBroadphaseOptimize;

#if ENABLE_PHYSICS_ANALYTICS == 1
        const int bitIndex = std::countr_zero(static_cast<unsigned int>(createSettings.mObjectLayer));
        ++mBodyCounts[bitIndex];
        if (createSettings.mMotionType == JPH::EMotionType::Static) {
            mDirtyStaticDebugRender = true;
        }
#endif

        return *body;
    }

    void removeBody(PhysBodyID id, bool shouldDestroy) {
        ASSERT_GAME_THREAD();
        JPH::BodyInterface& bodyInterface = getBodyInterfaceNonLocking();
#if ENABLE_PHYSICS_ANALYTICS == 1

        JPH::ObjectLayer layer = bodyInterface.GetObjectLayer(JPH::BodyID(id));
        const int bitIndex = std::countr_zero(static_cast<unsigned int>(layer));
        --mBodyCounts[bitIndex];
        if (bodyInterface.GetMotionType(JPH::BodyID(id)) == JPH::EMotionType::Static) {
            mDirtyStaticDebugRender = true;
        }
#endif

        // We have to clear the user data because removing a body not at rest
        // triggers a rest callback. By clearing the user data we ensure we wont listen for it
        bodyInterface.SetUserData(JPH::BodyID(id), {});
        bodyInterface.RemoveBody(JPH::BodyID(id));
        if (shouldDestroy) {
            bodyInterface.DestroyBody(JPH::BodyID(id));
        }
    }


#if ENABLE_PHYSICS_ANALYTICS == 1
    std::atomic_int mBodyCounts[PhysicsObjectLayer::COUNT] = {};
#endif

    JPH::PhysicsSystem& getSystem() {
        return physicsSystem;
    }
private:
    // Now we can create the actual physics system.
    JPH::PhysicsSystem physicsSystem;

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    JPH::TempAllocatorImpl tempAllocator = JPH::TempAllocatorImpl(10 * 1024 * 1024);

    PhysicsJobSystem jobSystem = PhysicsJobSystem(JPH::cMaxPhysicsBarriers);

    // Mapping table from object layer to broadphase layer
    BPLayerInterfaceImpl broadPhaseLayerInterface;

    // Filters object vs broadphase layers
    JPH::ObjectVsBroadPhaseLayerFilterMask objectVsBroadphaseLayerFilter;

    // Filters object vs object layers
    JPH::ObjectLayerPairFilterMask objectVsObjectLayerFilter;

    // Listen for activate/deactivate
    PhysicsBodyActivationListener bodyActivationListener;

    i32 mNewBodiesWithoutBroadphaseOptimize = 0;
    float mTimeSinceLastOptimization = 0.0f;
};

PhysicsWorld::PhysicsWorld(World& world, CollisionShapeRepository& shapeRepo) : mWorld(world), mShapeRepo(shapeRepo) {
    assert(JPH::Factory::sInstance);

    // PhysicsComponent relies on this
    if (mWorld.getNetMode() != WorldNetMode::Editor) {
        assert(!sGamePhysicsWorld);
        sGamePhysicsWorld = this;
    }

    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
    const uint cMaxBodies = 65536;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
    const uint cNumBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
    // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
    const uint cMaxBodyPairs = 65536;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
    // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
    const uint cMaxContactConstraints = 10240;

    mContext = std::make_unique<JPHPhysicsWorldContext>(*this);
    mContext->init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints);
}

PhysicsWorld::~PhysicsWorld() {

    if (sGamePhysicsWorld == this) {
        sGamePhysicsWorld = nullptr;
    }

    // Unregisters all types with the factory and cleans up the default material
    JPH::UnregisterTypes();

    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsWorld::initializeJPH() {
    assert(JPH::VerifyJoltVersionID());

    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
    // This needs to be done before any other Jolt function is called.
    JPH::RegisterDefaultAllocator();

    // Install trace and assert callbacks
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
    // It is not directly used in this example but still required.
    assert(!JPH::Factory::sInstance);
    JPH::Factory::sInstance = new JPH::Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
    // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
    // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
    JPH::RegisterTypes();

    // Shared query shapes
    sUnitSphereShape = std::make_unique<JPH::SphereShape>(1.0_r);
    sUnitSphereShape->SetUserData(PhysicsShapeUserData(CollisionShapes::Sphere));
    sUnitCylinderShape = std::make_unique<JPH::CylinderShape>(1.0_r, 1.0_r);
    sUnitCylinderShape->SetUserData(PhysicsShapeUserData(CollisionShapes::Cylinder));
}

int PhysicsWorld::stepSimulation(f32 deltaTime) {
    PROFILE_SCOPE(PHYSICS_STEP_PROFILE_NAME);

    // Fixed timestep
    constexpr f32 PHYSICS_TIMESTEP = 1.0f / 60.0f;

    // Step multiple times if needed
    f32 stepCountf;
    mTickTimeRemainder = modf((deltaTime + mTickTimeRemainder) / PHYSICS_TIMESTEP, &stepCountf);

    constexpr i32 MAX_COLLISION_STEPS_PER_FRAME = 2;
    const i32 collisionSteps = glm::min(static_cast<i32>(stepCountf), MAX_COLLISION_STEPS_PER_FRAME);

    // Step the world
    if (collisionSteps > 0) {
        mContext->update(PHYSICS_TIMESTEP, collisionSteps);
        updateItemEntitiesChangedThisFrame();
    }
    return collisionSteps;
}

void PhysicsWorld::updateTerrainBody(HeightmapPatch& patch) {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();

    // Adding an additional edge on right and north side to fill gap, which means we have to do
    constexpr i32 PADDED_WIDTH = HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1;
    constexpr i32 PADDED_SIZE = SQ(PADDED_WIDTH);

    const f32v3 cornerPos = patch.aabb.pos;
    const f32v2 centerPos = patch.aabb.getCenter();
    const f32 offsetToEdge = centerPos.x - cornerPos.x;

    // More efficient to manually construct settings so we avoid a full array copy
    JPH::HeightFieldShapeSettings settings;
    settings.mOffset = JPH::Vec3Arg(-offsetToEdge, 0.0f, -offsetToEdge);
    settings.mScale = JPH::Vec3Arg(HEIGHTMAP_QUAD_SIZE, HEIGHT_STEP /*This will uncompress*/, HEIGHTMAP_QUAD_SIZE);
    settings.mSampleCount = PADDED_WIDTH;
    settings.mHeightSamples.resize(PADDED_SIZE);
    auto& heights = settings.mHeightSamples;

    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const HeightmapPatch* rightPatch = heightGrid.getHeightDataAt(patch.id + 1);
    const HeightmapPatch* up = heightGrid.getHeightDataAt(patch.id + heightGrid.getWidthPatches());
    const HeightmapPatch* upRight = heightGrid.getHeightDataAt(patch.id + heightGrid.getWidthPatches() + 1);
    {
        PROFILE_SCOPE("Copy Height");
        for (int y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
            int yOffset = (PADDED_WIDTH - y - 1) * PADDED_WIDTH;
            for (int x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                // Need to flip Y due to handedness change
                const int i = yOffset + x;
                heights[yOffset + x] = (f32)patch.getCompressedHeightAt<true>(y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x);
            }
            // Right side grabs from neighbor
            if (rightPatch) [[likely]] {
                heights[yOffset + HEIGHTMAP_VERT_WIDTH_PER_PATCH] = (f32)rightPatch->getCompressedHeightAt<true>(y * HEIGHTMAP_VERT_WIDTH_PER_PATCH);
            }
            else {
                heights[yOffset + HEIGHTMAP_VERT_WIDTH_PER_PATCH] = heights[yOffset + HEIGHTMAP_VERT_WIDTH_PER_PATCH - 1];
            }
        }
        // Up patch
        if (up) [[likely]] {
            for (int x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                heights[x] = (f32)up->getCompressedHeightAt<true>(x);
            }
        }
        else {
            for (int x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                heights[x] = heights[x - PADDED_WIDTH];
            }
        }
        // UpRight
        if (upRight) [[likely]] {
            heights[HEIGHTMAP_VERT_WIDTH_PER_PATCH] = (f32)upRight->getCompressedHeightAt<true>(0);
        }
        else {
            heights[HEIGHTMAP_VERT_WIDTH_PER_PATCH] = heights[HEIGHTMAP_VERT_WIDTH_PER_PATCH - 1];
        }
    }

    JPH::ShapeSettings::ShapeResult newShape = settings.Create();
    // Update old body
    if (patch.physBodyID != INVALID_PHYS_BODY_ID) {
        mContext->updateShape(patch.physBodyID, newShape.Get(), false, JPH::EActivation::DontActivate);
    }
    else {
        patch.physBodyID = createTerrainBody(f32v3(centerPos.x, centerPos.y, 0.0f), newShape.Get());
    }
}

PhysBodyID PhysicsWorld::createDynamicItemBody(entt::entity ownerEntity, f32v3 position, CollisionShapeID shapeId, glm::quat orientation, f32v3 linearVelocity, f32v3 angularVelocity) {
    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, shapeId, JPH::EMotionType::Dynamic,
        makeObjectLayerMasked(
            PhysicsObjectLayer::DynamicItem,
            PhysicsObjectLayer::Static | PhysicsObjectLayer::DynamicItem
        ), 1.0f /*scale*/
    );
    createSettings.mUserData = PhysicsBodyUserData(ownerEntity, PhysicsBodyUserDataType::ItemEntity);
    createSettings.mRotation = JPH::Quat(orientation.x, orientation.y, orientation.z, orientation.w);
    createSettings.mLinearVelocity = JPH::Vec3(linearVelocity.x, linearVelocity.y, linearVelocity.z);
    createSettings.mAngularVelocity = JPH::Vec3(angularVelocity.x, angularVelocity.y, angularVelocity.z);
    createSettings.mFriction = 1.0f;
    createSettings.mAngularDamping = 0.4f;
    createSettings.mLinearDamping = 0.2f;

    return createEntityBody(createSettings, ownerEntity, shapeId);
}

PhysBodyID PhysicsWorld::createStaticItemBody(entt::entity ownerEntity, f32v3 position, CollisionShapeID shapeId, glm::quat orientation, f32 scale) {
    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, shapeId, JPH::EMotionType::Static,
        makeObjectLayerMasked(
            PhysicsObjectLayer::Static,
            PhysicsObjectLayer::DynamicItem
        ), scale
    );
    createSettings.mUserData = PhysicsBodyUserData(ownerEntity, PhysicsBodyUserDataType::ItemEntity);
    // TODO: Proper orientation
    createSettings.mRotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI * 0.5f) * JPH::Quat(orientation.x, orientation.y, orientation.z, orientation.w);

    return createEntityBody(createSettings, ownerEntity, shapeId);
}

PhysBodyID PhysicsWorld::createCharacterBody(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents) {
    CollisionShapeID shapeId = mShapeRepo.getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.y);

    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, shapeId, JPH::EMotionType::Dynamic, 
        makeObjectLayerMasked(PhysicsObjectLayer::Character, PhysicsObjectLayer::Static | PhysicsObjectLayer::Character), 1.0f /*scale*/
    );
    createSettings.mUserData = PhysicsBodyUserData(ownerEntity);
    createSettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;

    return createEntityBody(createSettings, ownerEntity, shapeId);
}

std::unique_ptr<JPH::CharacterBase> PhysicsWorld::createSimpleCharacter(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents) {
    ASSERT_GAME_THREAD();

    CollisionShapeID shapeId = mShapeRepo.getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.y);
    const JPH::Shape* shape = mShapeRepo.getShape(shapeId);

    JPH::CharacterSettings settings;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(60.0f);
    settings.mLayer = makeObjectLayerMasked(
        PhysicsObjectLayer::Character | PhysicsObjectLayer::DynamicSolid, // layerBits
        PhysicsObjectLayer::Static | PhysicsObjectLayer::Character // collideMaskBits
    );
    settings.mShape = shape;
    settings.mFriction = 0.5f;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisZ(), -halfExtents.y); // Accept contacts that touch the lower sphere of the capsule
    std::unique_ptr<JPH::Character> newCharacter = std::make_unique<JPH::Character>(
        &settings,
        JPH::RVec3(position.x, position.y, position.z),
        ROTATE_ZUP,
        PhysicsBodyUserData(ownerEntity),
        &mContext->getSystem()
    );
    newCharacter->AddToPhysicsSystem(JPH::EActivation::Activate);
    return newCharacter;
}

void PhysicsWorld::updateTileContainerMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();

    auto&& it = mTileContainerPhysicsData.find(meshBuilder.getOwnerTileContainerID());

    // Remove old collision and return
    if (!meshBuilder.hasAnyCollision()) {
        if (it != mTileContainerPhysicsData.end()) {
            for (auto& [key, physBodyID] : it->second->mTileKeyToPhysBodyID) {
                mContext->removeBody(physBodyID, true);
            }

            PhysBodyID& staticMesh = it->second->mStaticMesh;
            if (staticMesh != INVALID_PHYS_BODY_ID) {
                mContext->removeBody(staticMesh, true);
                staticMesh = INVALID_PHYS_BODY_ID;
            }

            mTileContainerPhysicsData.erase(it);
        }
        return;
    }

    // Update rigid bodies and cache static mesh pointer
    PhysBodyID* staticMesh;
    if (it == mTileContainerPhysicsData.end()) {
        // Simply creating brand new collision
        NewTileContainerPhysicsData& newData = *mTileContainerPhysicsData.emplace(meshBuilder.getOwnerTileContainerID(), std::make_unique<NewTileContainerPhysicsData>()).first->second;
        staticMesh = &newData.mStaticMesh;
        addTrackedStaticRigidBodiesFromGatherer(meshBuilder.mTrackedRigidBodyGatherer, newData);
    }
    else {
        // May remove some old collision
        NewTileContainerPhysicsData& data = *it->second;
        staticMesh = &data.mStaticMesh;
        updateTrackedStaticRigidBodiesFromGatherer(meshBuilder.mTrackedRigidBodyGatherer, data);
    }

    // Update procedural mesh
    if (meshBuilder.mVerts.size()) {
        JPH::MeshShapeSettings shapeSettings = createStaticMeshShapeSettings(meshBuilder.mVerts, meshBuilder.mIndices);
        JPH::Shape* shape = shapeSettings.Create().Get();
        shape->SetUserData(PhysicsShapeUserData(CollisionShapes::Mesh));

        // Create body or update its shape
        if (*staticMesh == INVALID_PHYS_BODY_ID) {
            const f32v3 rootPos = meshBuilder.getRootPos();
            JPH::BodyCreationSettings createSettings(
                shape, JPH::RVec3(rootPos.x, rootPos.y, rootPos.z), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                makeObjectLayerMasked(
                    PhysicsObjectLayer::Static,  // layerBits
                    PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem // collideMaskBits
                )
            );
            JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::DontActivate, 434343 /*Cheeky mesh number for debug output*/);
            body.SetUserData(PhysicsBodyUserData(meshBuilder.getOwnerTileContainerID()));
            *staticMesh = body.GetID().GetIndexAndSequenceNumber();
        }
        else {
            mContext->updateShape(*staticMesh, shape, false, JPH::EActivation::DontActivate);
        }
    }
    else if (*staticMesh != INVALID_PHYS_BODY_ID) {
        mContext->removeBody(*staticMesh, true);
        *staticMesh = INVALID_PHYS_BODY_ID;
    }
}

void PhysicsWorld::removeBody(PhysBodyID id, bool shouldDestroy) {
    assert(id != INVALID_PHYS_BODY_ID);
    mContext->removeBody(id, shouldDestroy);
}

PhysHitResult PhysicsWorld::raycastFirst(
    f32v3 rayStart,
    f32v3 rayEnd,
    const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter /*= {}*/,
    const JPH::ObjectLayerFilter& objectLayerFilter /*= {}*/,
    const JPH::BodyFilter& bodyFilter /*= {}*/,
    bool traceFarTerrain /*= false*/) const
{
    PhysHitResult rv;

    const JPH::NarrowPhaseQuery* queryApi;
    const JPH::BodyInterface* bodyInterface;
    // Game thread can be lockless since only game thread writes
    if (IS_GAME_THREAD()) {
        queryApi = &mContext->getSystem().GetNarrowPhaseQueryNoLock();
        bodyInterface = &mContext->getSystem().GetBodyInterfaceNoLock();
    }
    else {
        queryApi = &mContext->getSystem().GetNarrowPhaseQuery();
        bodyInterface = &mContext->getSystem().GetBodyInterface();
    }

    const f32v3 dir = rayEnd - rayStart;
    JPH::RRayCast ray(JPH::RVec3(rayStart.x, rayStart.y, rayStart.z), JPH::Vec3(dir.x, dir.y, dir.z));
    JPH::RayCastResult castResult;

    if (queryApi->CastRay(
        ray,
        castResult,
        broadPhaseLayerFilter,
        objectLayerFilter,
        bodyFilter
    )) {
        rv.mHitBody = castResult.mBodyID.GetIndexAndSequenceNumber();
        rv.mPosition = rayStart + castResult.mFraction * (rayEnd - rayStart);
        rv.mNormal = dir;
        rv.mTime = castResult.mFraction;
        // TODO: In the non game thread case we could use the lock interface so we don't lock twice
        rv.mBodyUserData = bodyInterface->GetUserData(castResult.mBodyID);
        rv.mShape = bodyInterface->GetShape(castResult.mBodyID);
        rv.mPenetrationDepth = 0.0f;

#ifdef JPH_DEBUG_RENDERER
        if (sPhysicsDebugRenderer && sPhysicsDebugRenderer->mRenderSettings.showRaycasts) {
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(rayStart, rv.mPosition, color::Green, sPhysicsDebugRenderer->mRenderSettings.queryRenderTime);
            AM::DebugRenderer::drawWireQuadThreadSafe(rv.mPosition - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.2f), color::Green, sPhysicsDebugRenderer->mRenderSettings.queryRenderTime);
        }
#endif

    }
    else if (traceFarTerrain && broadPhaseLayerFilter.ShouldCollide(BroadPhaseLayers::Static)) {
        // Try manual terrain query for long queries so we can hit far terrain
        IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
        HeightmapPickResult result = heightGrid.pick(rayStart, rayEnd);
        if (const HeightmapPatch* patch = heightGrid.getHeightDataAtWorldPos(i32v2(result.hitPoint))) {
            rv.mHitBody = patch->physBodyID;
            rv.mBodyUserData = bodyInterface->GetUserData(JPH::BodyID(rv.mHitBody));
            rv.mShape = bodyInterface->GetShape(JPH::BodyID(rv.mHitBody));
            rv.mPenetrationDepth = 0.0f;
        }
        rv.mPosition = result.hitPoint;
        rv.mNormal = result.hitNormal;
        rv.mTime = result.hitTime;

#ifdef JPH_DEBUG_RENDERER
        if (sPhysicsDebugRenderer && sPhysicsDebugRenderer->mRenderSettings.showRaycasts) {
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(rayStart, rv.mPosition, color::Green, sPhysicsDebugRenderer->mRenderSettings.queryRenderTime);
            AM::DebugRenderer::drawWireQuadThreadSafe(rv.mPosition - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.2f), color::Green, sPhysicsDebugRenderer->mRenderSettings.queryRenderTime);
        }
#endif
    }
#ifdef JPH_DEBUG_RENDERER
    else {
        if (sPhysicsDebugRenderer && sPhysicsDebugRenderer->mRenderSettings.showRaycasts) {
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(rayStart, rayEnd, color::Red, sPhysicsDebugRenderer->mRenderSettings.queryRenderTime);
        }
    }
#endif

    return rv;
}

int PhysicsWorld::queryObjectsInAABB(
    f32v3 min,
    f32v3 max,
    std::span<PhysicsQueryResult> outResults,
    const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter /*= {}*/,
    const JPH::ObjectLayerFilter& objectLayerFilter /*= {}*/,
    const JPH::BodyFilter& bodyFilter /*= {}*/
) {
    ASSERT_GAME_THREAD();
    PhysicsQueryCollector shapeCollector(outResults, mContext->getBodyInterfaceNonLocking());
    const JPH::NarrowPhaseQuery& queryApi = mContext->getSystem().GetNarrowPhaseQueryNoLock();

    queryApi.CollectTransformedShapes(
        JPH::AABox(JPH::Vec3(min.x, min.y, min.z), JPH::Vec3(max.x, max.y, max.z)),
        shapeCollector,
        broadPhaseLayerFilter,
        objectLayerFilter,
        bodyFilter
    );

    return shapeCollector.collectedShapes;
}

int PhysicsWorld::collideSphere(
    f32v3 center,
    f32 radius,
    std::span<PhysHitResult> outResults,
    const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter /*= {}*/,
    const JPH::ObjectLayerFilter& objectLayerFilter /*= {}*/,
    const JPH::BodyFilter& bodyFilter /*= {}*/
) {
    ASSERT_GAME_THREAD();

    PhysicsHitCollector hitCollector(outResults, mContext->getBodyInterfaceNonLocking());
    const JPH::NarrowPhaseQuery& queryApi = mContext->getSystem().GetNarrowPhaseQueryNoLock();

    const JPH::DVec3 centerD(center.x, center.y, center.z);

    queryApi.CollideShape(
        sUnitSphereShape.get(),
        JPH::Vec3(radius, radius, radius),
        JPH::DMat44::sTranslation(centerD),
        JPH::CollideShapeSettings(),
        JPH::DVec3::sZero(),
        hitCollector,
        broadPhaseLayerFilter,
        objectLayerFilter,
        bodyFilter
    );
    return hitCollector.collectedHits;
}

int PhysicsWorld::collideCylinder(
    f32v3 center,
    f32v2 halfDims,
    std::span<PhysHitResult> outResults,
    const JPH::BroadPhaseLayerFilter& broadPhaseLayerFilter /*= {}*/,
    const JPH::ObjectLayerFilter& objectLayerFilter /*= {}*/,
    const JPH::BodyFilter& bodyFilter /*= {}*/
) {
    ASSERT_GAME_THREAD();

    PhysicsHitCollector hitCollector(outResults, mContext->getBodyInterfaceNonLocking());
    const JPH::NarrowPhaseQuery& queryApi = mContext->getSystem().GetNarrowPhaseQueryNoLock();

    const JPH::DVec3 centerD(center.x, center.y, center.z);

    queryApi.CollideShape(
        sUnitCylinderShape.get(),
        JPH::Vec3(halfDims.x, halfDims.y, 1.0f),
        JPH::DMat44::sRotationTranslation(ROTATE_ZUP, centerD),
        JPH::CollideShapeSettings(),
        JPH::DVec3::sZero(),
        hitCollector,
        broadPhaseLayerFilter,
        objectLayerFilter,
        bodyFilter
    );
    return hitCollector.collectedHits;
}

#ifdef JPH_DEBUG_RENDERER
// For debug draw
SERIALIZABLE_ENUM(JPH::BodyManager::EShapeColor, EShapeColor,
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, InstanceColor),
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, ShapeTypeColor),
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, MotionTypeColor),
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, SleepColor),
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, IslandColor),
    ENUM_FIELD_SIMPLE(JPH::BodyManager::EShapeColor, MaterialColor)
);
#endif

void PhysicsWorld::updateAndRenderImguiDebugControls() {

#ifdef JPH_DEBUG_RENDERER
    bool changed = false;

    changed |= ImGui::Checkbox("Debug Draw", &sPhysicsDebugRenderer->mRenderSettings.enableDebugDraw);

    if (sPhysicsDebugRenderer->mRenderSettings.enableDebugDraw) {
        changed |= ImGui::Checkbox("  Shapes", &sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawShape);
        changed |= ImguiUtil::EnumCombo("  Shape Color", sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawShapeColor);
        changed |= ImGui::Checkbox("  Wireframe", &sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawShapeWireframe);
        changed |= ImGui::Checkbox("  Bounding Boxes", &sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawBoundingBox);
        changed |= ImGui::Checkbox("  Center Of Mass", &sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawCenterOfMassTransform);
        changed |= ImGui::Checkbox("  Velocity", &sPhysicsDebugRenderer->mRenderSettings.bodyDrawSettings.mDrawVelocity);
        changed |= ImGui::SliderFloat("  Render Alpha", &sPhysicsDebugRenderer->mRenderSettings.alpha, 0.0f, 1.0f);
    }

    ImGui::SeparatorText("Queries");
    // These dont dirty the render
    ImGui::Checkbox("Show Raycasts", &sPhysicsDebugRenderer->mRenderSettings.showRaycasts);
    ImGui::Checkbox("Show Shape Queries", &sPhysicsDebugRenderer->mRenderSettings.showShapeQueries);
    if (sPhysicsDebugRenderer->mRenderSettings.showRaycasts || sPhysicsDebugRenderer->mRenderSettings.showShapeQueries) {
        ImGui::SliderInt("Query Draw Time", &sPhysicsDebugRenderer->mRenderSettings.queryRenderTime, 1, 512);
    }


    if (changed) {
        mContext->mDirtyStaticDebugRender = true;
    }
#endif


    ImGui::Spacing();
    ImGui::SeparatorText("Stats");
    ImGui::Text("   Item Bodies: %d", getBodyCount(PhysicsObjectLayer::DynamicItem));
    ImGui::Text("   Character Bodies: %d", getBodyCount(PhysicsObjectLayer::Character));
    ImGui::Text("   Static Bodies: %d", getBodyCount(PhysicsObjectLayer::Static));
    InstrumentorDebugStrings debugStr = Instrumentor::get().getSingleResult(GAME_THREAD_ID, PHYSICS_STEP_PROFILE_NAME);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), debugStr.name.c_str());

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 50, 255, 255));
    ImGui::Text(debugStr.avg.c_str());
    ImGui::Text(debugStr.max.c_str());
    ImGui::PopStyleColor();
}

void PhysicsWorld::debugRender(const Camera3D& camera) const {
#ifdef JPH_DEBUG_RENDERER
    PROFILE_FUNCTION();
    if (!sPhysicsDebugRenderer) {
        sPhysicsDebugRenderer = std::make_unique<PhysicsDebugRenderer>();
    }
    mContext->debugDraw(camera);
#endif //JPH_DEBUG_RENDERER
}

JPH::ObjectLayer PhysicsWorld::makeObjectLayerMasked(JPH::ObjectLayer layerBits, JPH::ObjectLayer collideMaskBits) {
    return JPH::ObjectLayerPairFilterMask::sGetObjectLayer(layerBits, collideMaskBits);
}

#if ENABLE_PHYSICS_ANALYTICS == 1
int PhysicsWorld::getBodyCount(JPH::ObjectLayer layer) const {
    const int bitIndex = std::countr_zero(static_cast<unsigned int>(layer));
    return mContext->mBodyCounts[bitIndex];
}
#endif

JPH::BodyCreationSettings PhysicsWorld::makeBodyCreateSettings(f32v3 position, CollisionShapeID shapeId, JPH::EMotionType motionType, JPH::ObjectLayer layer, f32 scale) {
    if (scale != 1.0f) {
        return JPH::BodyCreationSettings(
            JPH::RefConst<JPH::ScaledShapeSettings>(new JPH::ScaledShapeSettings(mShapeRepo.getShape(shapeId), JPH::Vec3(scale, scale, scale))),
            JPH::RVec3(position.x, position.y, position.z),
            ROTATE_ZUP, 
            motionType, 
            layer
        );
    }
    else {
        return JPH::BodyCreationSettings(mShapeRepo.getShape(shapeId), JPH::RVec3(position.x, position.y, position.z), ROTATE_ZUP, motionType, layer);
    }
}

PhysBodyID PhysicsWorld::createEntityBody(const JPH::BodyCreationSettings& createSettings, entt::entity ownerEntity, CollisionShapeID shapeId) {
    ASSERT_GAME_THREAD();
    // Userdata set by caller
    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::Activate, shapeId);
    return body.GetID().GetIndexAndSequenceNumber();
}

PhysBodyID PhysicsWorld::createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, f32q orientation, ModelID modelId) {
    ASSERT_GAME_THREAD();

    const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);
    assert(modelDef.mCollisionShapeID != INVALID_COLLISION_SHAPE_ID);

    // Offset shape to proper root. Include scale here IF we have scale
    position += orientation * modelDef.mColliderData.mBaseOffset /* *scale*/;
    // Combine shape orientation with input orientation
    orientation *= modelDef.mColliderData.mBaseOrientation;

    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(
        position, modelDef.mCollisionShapeID, JPH::EMotionType::Static,
        makeObjectLayerMasked(PhysicsObjectLayer::Static, PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem), 1.0f /*scale*/
    );
    createSettings.mUserData = PhysicsBodyUserData(containerId, tileIndex);
    createSettings.mRotation = JPH::Quat(orientation.x, orientation.y, orientation.z, orientation.w);

    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::DontActivate, modelDef.mCollisionShapeID);
    return body.GetID().GetIndexAndSequenceNumber();
}

PhysBodyID PhysicsWorld::createTerrainBody(f32v3 position, JPH::Shape* terrainShape) {
    ASSERT_GAME_THREAD();
    terrainShape->SetUserData(PhysicsShapeUserData(CollisionShapes::Terrain));
    JPH::BodyCreationSettings createSettings(
        terrainShape, JPH::RVec3(position.x, position.y, position.z), ROTATE_ZUP, JPH::EMotionType::Static,
        makeObjectLayerMasked(PhysicsObjectLayer::Static, PhysicsObjectLayer::DynamicSolid | PhysicsObjectLayer::DynamicItem)
    );
    createSettings.mUserData = PhysicsBodyUserData(PhysicsBodyUserDataType::Terrain);

    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::DontActivate, 696969 /*Cheeky terrain number for debug output*/);
    return body.GetID().GetIndexAndSequenceNumber();
}

JPH::MeshShapeSettings PhysicsWorld::createStaticMeshShapeSettings(std::span<f32v3> verts, std::span<ui32> indices) {
    ASSERT_GAME_THREAD();
    assert(indices.size() % 3 == 0);
    assert(indices.size() && verts.size());

    JPH::VertexList jpVerts;
    jpVerts.reserve(verts.size());
    for (const f32v3& vert : verts) {
        jpVerts.emplace_back(vert.x, vert.y, vert.z);
    }
    // Triangles must be provided in counter clockwise order.
    // For simulation, the triangles are considered to be single sided.
    JPH::IndexedTriangleList jpInds;
    jpInds.reserve(indices.size() / 3);
    for (size_t i = 0; i < indices.size(); i += 3) {
        jpInds.emplace_back(indices[i], indices[i + 1], indices[i + 2]);
    }
    
    return JPH::MeshShapeSettings(std::move(jpVerts), std::move(jpInds));
}

void PhysicsWorld::addTrackedStaticRigidBodiesFromGatherer(TrackedStaticModelColliderGatherer& gatherer, NewTileContainerPhysicsData& physicsData) {
    PROFILE_FUNCTION();
    assert(physicsData.mTileKeyToPhysBodyID.empty());

    for (auto& it : gatherer.mRigidBodiesToAdd) {
        const TileKey key = TileKey{ it.ownerTilePosition, it.tileId, it.layer };
        physicsData.mTileKeyToPhysBodyID.emplace(key, createTileBody(gatherer.mContainerId, it.ownerTilePosition, it.position, it.orientation, it.modelId));
    }
}

void PhysicsWorld::updateTrackedStaticRigidBodiesFromGatherer(TrackedStaticModelColliderGatherer& gatherer, NewTileContainerPhysicsData& physicsData) {
    PROFILE_FUNCTION();

    // TODO: Scratch allocator?
    static thread_local UnorderedFlatSet<TileKey> addedKeys;
    addedKeys.reserve(gatherer.mRigidBodiesToAdd.size());

    for (auto& it : gatherer.mRigidBodiesToAdd) {
        const TileKey key = TileKey{ it.ownerTilePosition, it.tileId, it.layer };
        auto&& pit = physicsData.mTileKeyToPhysBodyID.find(key);
        // Only add if it doesn't already exist
        if (pit == physicsData.mTileKeyToPhysBodyID.end()) {
            physicsData.mTileKeyToPhysBodyID.emplace(key, createTileBody(gatherer.mContainerId, it.ownerTilePosition, it.position, it.orientation, it.modelId));
        }
        addedKeys.insert(key);
    }
    // Remove any keys that are not in the gatherer
    for (auto it = physicsData.mTileKeyToPhysBodyID.begin(); it != physicsData.mTileKeyToPhysBodyID.end();) {
        if (!addedKeys.contains(it->first)) {
            mContext->removeBody(it->second, true);
            it = physicsData.mTileKeyToPhysBodyID.erase(it);
        }
        else {
            ++it;
        }
    }
    addedKeys.clear();
}


void PhysicsWorld::updateItemEntitiesChangedThisFrame() {
    PROFILE_FUNCTION();

    for (entt::entity entity : mItemEntitiesMovedThisFrame) {
        PhysicsWorldEvent event{ entity };
        dispatchItemMoved(event);
#ifdef DEBUG
        // Make sure we had no duplicates
        assert(!mItemEntitiesRestedThisFrame.contains(entity));
#endif
    }

    for (entt::entity entity : mItemEntitiesRestedThisFrame) {
        PhysicsWorldEvent event{ entity };
        dispatchItemAtRest(event);
#ifdef DEBUG
        // Make sure we had no duplicates
        assert(!mItemEntitiesMovedThisFrame.contains(entity));
#endif
    }

    mItemEntitiesRestedThisFrame.clear();
    mItemEntitiesMovedThisFrame.clear();
}

const JPH::BodyLockInterface& PhysicsWorld::getBodyLockInterface() const {
    return mContext->getBodyLockInterface();
}

JPH::BodyInterface& PhysicsWorld::getBodyInterface() const {
    return mContext->getBodyInterface();
}

const JPH::BodyInterface& PhysicsWorld::getBodyInterfaceNonLocking() const {
    return mContext->getBodyInterfaceNonLocking();
}

//ASSERT_GAME_THREAD();
//PROFILE_FUNCTION();
//btVector3 start = f32v3ToBtVector3(rayStart);
//btVector3 end = f32v3ToBtVector3(rayEnd);
//// TODO: Use more of btCollisionWorld::ClosestRayResultCallback?
//CustomRayResult rayResult(start, end);
//int collisionMask = 0;
//
//if (pickTypes & PICK_TYPE_DYNAMIC) {
//    collisionMask |= COLLISION_FILTER_DYNAMIC;
//}
//if (pickTypes & PICK_TYPE_STATIC) {
//    collisionMask |= COLLISION_FILTER_STATIC;
//}
//rayResult.m_collisionFilterGroup = BIT_CAST(CollisionGroup::QUERY);
//rayResult.m_collisionFilterMask = collisionMask;
////rayResult.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
//mDynamicsWorld->rayTest(start, end, rayResult);
//
//// Test terrain
//if (pickTypes & PICK_TYPE_STATIC) {
//    HeightmapPickResult result = mWorld.getHeightmapGrid().pick(rayStart, rayEnd);
//    if (result.hitTime < rayResult.m_closestHitFraction) {
//        PhysHitResult rv;
//        rv.mTime = result.hitTime;
//        rv.mNormal = result.hitNormal;
//        rv.mCollisionObject = nullptr;
//        rv.mPosition = result.hitPoint;
//        assert(rv.mTime >= 0.0);
//        return rv;
//    }
//}
//
//PhysHitResult rv;
//rv.mTime = rayResult.m_closestHitFraction;
//rv.mNormal = rayResult.mHitNormal;
//rv.mCollisionObject = rayResult.m_collisionObject;
//rv.mPosition = rayStart + (rayEnd - rayStart) * rv.mTime;
//if (queryFlags.isBitSet(PhysicsPickQueryFlags::QUERY_TILE_INFO) && rv.mCollisionObject) {
//
//    if (rv.mCollisionObject->getUserIndex() != INVALID_PHYSICS_USER_INDEX) {
//        rv.mSelectedEntity = entt::entity(rv.mCollisionObject->getUserIndex());
//    }
//    else {
//        TileContainerID containerId = rv.mCollisionObject->getUserIndex2();
//        if (containerId != INVALID_PHYSICS_USER_INDEX) {
//            rv.mContainerID = containerId;
//            TileIndex index = rv.mCollisionObject->getUserIndex3();
//            if (index != INVALID_PHYSICS_USER_INDEX) {
//                rv.mTileIndex = index;
//            }
//            else {
//                f32v2 tilePos2D(rv.mPosition.x, rv.mPosition.y);
//                TileHandle handle = mWorld.getTerrainTileHandleAtWorldPos(tilePos2D);
//                if (handle.isValid()) {
//                    assert(tilePos2D.x >= 0.0f && tilePos2D.y >= 0.0f);
//                    if (Building* structure = mWorld.tryGetStructureAtWorldPos(TileCoord(tilePos2D))) {
//                        TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(rv.mPosition);
//                        if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
//                            rv.mTileIndex = nextHandle.tileIndex;
//                        }
//                    }
//                    /*Chunk* chunk = handle.container->getOwnerChunk();
//                    if (chunk->isDataReady()) {
//                        StructureArrayPtr structures = chunk->getStructuresAt(handle.tileIndex);
//                        for (int i = 0; i < structures.second; ++i) {
//                            Structure* structure = structures.first[i];
//                            TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(rv.mPosition);
//                            if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
//                                rv.mTileIndex = nextHandle.tileIndex;
//                                break;
//                            }
//                        }
//                    }*/
//                }
//                else {
//                    // Need to query which tile we selected
//                    LOG_DEBUG("Selected invalid chunk in ray pick");
//                }
//            }
//        }
//    }
//}
//
//assert(rv.mTime >= 0.0);
//return rv;