#include "stdafx.h"
#include "NewPhysicsWorld.h"

// Jolt Documentation
// https://jrouwe.github.io/JoltPhysics/index.html

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/Character.h>

#include "physics/PhysicsDebugRenderer.h"
#include "physics/StaticPhysicsMeshBuilder.h"
#include "physics/CollisionShapeRepository.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"

#include "terrain/HeightmapPatch.h"

#include "options/DebugOptions.h"


PhysicsBodyUserData::PhysicsBodyUserData(entt::entity owner) {
    data = e_cast(PhysicsBodyUserDataType::Entity) << 62 | static_cast<ui64>(owner);
}

PhysicsBodyUserData::PhysicsBodyUserData(TileContainerID tileContainer, TileIndex tileIndex) {
    assert(tileIndex <= 0x3FFFFFFF); // 30 bits
    data = e_cast(PhysicsBodyUserDataType::Tile) << 62 | (static_cast<ui64>(tileIndex) << 32) | static_cast<ui64>(tileContainer);
}

PhysicsBodyUserDataType PhysicsBodyUserData::getType() const {
    return static_cast<PhysicsBodyUserDataType>(data >> 62);
}

std::pair<TileContainerID, TileIndex> PhysicsBodyUserData::getTileData() const {
    assert(getType() == PhysicsBodyUserDataType::Tile);
    std::pair<TileContainerID, TileIndex> result;
    result.first = static_cast<TileContainerID>(data & 0xFFFFFFFF);
    result.second = static_cast<TileIndex>((data >> 32) & 0x3FFFFFFF);
    return result;
}

entt::entity PhysicsBodyUserData::getEntity() const {
    assert(getType() == PhysicsBodyUserDataType::Entity);
    return static_cast<entt::entity>(data & 0x3FFFFFFFFFFFFFFF);
}

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

static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
    panic("PHYSICS ASSERT FAILED: {}:{}: ({}) {}", inFile, inLine, inExpression, (inMessage != nullptr ? inMessage : ""));

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS


/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case e_cast(PhysicsObjectLayer::Static):
                return inObject2 == e_cast(PhysicsObjectLayer::Dynamic); // Non moving only collides with moving
            case e_cast(PhysicsObjectLayer::Dynamic):
                return true; // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer Static(0);
    static constexpr JPH::BroadPhaseLayer Dynamic(1);
    static constexpr uint Count(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl() {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[e_cast(PhysicsObjectLayer::Static)] = BroadPhaseLayers::Static;
        mObjectToBroadPhase[e_cast(PhysicsObjectLayer::Dynamic)] = BroadPhaseLayers::Dynamic;
    }

    virtual uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::Count;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < e_cast(PhysicsObjectLayer::COUNT));
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Dynamic:	return "Movable";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Static:	return "Static";
            default:													JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[e_cast(PhysicsObjectLayer::COUNT)];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case e_cast(PhysicsObjectLayer::Static):
                return inLayer2 == BroadPhaseLayers::Dynamic;
            case e_cast(PhysicsObjectLayer::Dynamic):
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// Can be called from multiple threads
class MyContactListener : public JPH::ContactListener {
public:
    // See: ContactListener
    virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override  {
        LOG_INFO("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
        LOG_INFO("Contact added callback");
    }

    virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
        LOG_INFO("Contact persisted callback");
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
        LOG_INFO("Contact removed callback");
    }
};

// Can be called from multiple threads
//class MyBodyActivationListener : public JPH::BodyActivationListener
//{
//public:
//    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, ui64 inBodyUserData) override {
//        LOG_INFO("A body got activated");
//    }
//
//    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, ui64 inBodyUserData) override {
//        LOG_INFO("A body went to sleep");
//    }
//};

class DynamicBodyFilter : public JPH::BodyDrawFilter {
public:
    bool ShouldDraw(const JPH::Body& inBody) const {
        return inBody.GetMotionType() == JPH::EMotionType::Dynamic;
    }
};

class StaticBodyFilter : public JPH::BodyDrawFilter {
public:
    bool ShouldDraw(const JPH::Body& inBody) const {
        return inBody.GetMotionType() == JPH::EMotionType::Static;
    }
};

class JPHPhysicsWorldContext {
public:
    void init(ui32 maxBodies, ui32 numBodyMutexes, ui32 maxBodyPairs, ui32 maxContactConstraints) {
        physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints, broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);
        // Z up
        physicsSystem.SetGravity(JPH::Vec3(0.0f, 0.0f, -9.81f));

        //physics_system.SetBodyActivationListener(&body_activation_listener);
        //physics_system.SetContactListener(&contact_listener);
    }

    JPH::BodyInterface& getBodyInterface() {
        return physicsSystem.GetBodyInterface();
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
        if (sDebugRenderer->mRenderSettings.showCollision) {

            JPH::BodyManager::DrawSettings settings;
            settings.mDrawShapeWireframe = sDebugRenderer->mRenderSettings.wireframe;

            sDebugRenderer->PrepareFrame(camera);
            // Only update static when static changes
            if (mDirtyStaticDebugRender) {
                StaticBodyFilter staticFilter;
                sDebugRenderer->PreDraw(true);
                physicsSystem.DrawBodies(settings, sDebugRenderer.get(), &staticFilter);
                mDirtyStaticDebugRender = false;
            }
            // Always update dynamic
            DynamicBodyFilter dynamicFilter;
            sDebugRenderer->PreDraw(false);
            physicsSystem.DrawBodies(settings, sDebugRenderer.get(), &dynamicFilter);
            sDebugRenderer->EndFrame();
        }
    }

    bool mDirtyStaticDebugRender = true;
#endif

    void updateShape(BodyID id, const JPH::Shape* newShape, bool updateMass, JPH::EActivation activateMode) {
        ASSERT_GAME_THREAD();
        JPH::BodyInterface& bodyInterface = getBodyInterface();

        bodyInterface.SetShape(JPH::BodyID(id), newShape, updateMass, activateMode);
#if ENABLE_PHYSICS_ANALYTICS == 1
        if (bodyInterface.GetMotionType(JPH::BodyID(id)) == JPH::EMotionType::Static) {
            mDirtyStaticDebugRender = true;
        }
#endif
    }

    JPH::Body& createBody(const JPH::BodyCreationSettings& createSettings, JPH::EActivation inActivationMode, CollisionShapeID shapeId) {
        ASSERT_GAME_THREAD();
        JPH::BodyInterface& bodyInterface = getBodyInterface();
        JPH::Body* body = bodyInterface.CreateBody(createSettings);
        if (body == nullptr) {
            panic("Failed to create entity body with shape ID {}", (int)shapeId);
        }
        bodyInterface.AddBody(body->GetID(), inActivationMode);
        ++mNewBodiesWithoutBroadphaseOptimize;

#if ENABLE_PHYSICS_ANALYTICS == 1
        ++mBodyCounts[createSettings.mObjectLayer];
        if (createSettings.mMotionType == JPH::EMotionType::Static) {
            mDirtyStaticDebugRender = true;
        }
#endif

        return *body;
    }

    void removeBody(PhysBodyID id) {
        getBodyInterface().RemoveBody(JPH::BodyID(id));
    }


#if ENABLE_PHYSICS_ANALYTICS == 1
    std::atomic_int mBodyCounts[e_count(PhysicsObjectLayer)] = {};
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

    // TODO: Use our job system instead
    JPH::JobSystemThreadPool jobSystem = JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::max(2u, std::thread::hardware_concurrency() - 3));

    // Mapping table from object layer to broadphase layer
    BPLayerInterfaceImpl broadPhaseLayerInterface;

    // Filters object vs broadphase layers
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseLayerFilter;

    // Filters object vs object layers
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter;

    i32 mNewBodiesWithoutBroadphaseOptimize = 0;
    float mTimeSinceLastOptimization = 0.0f;
};


NewPhysicsWorld::NewPhysicsWorld(World& world, CollisionShapeRepository& shapeRepo) : mWorld(world), mShapeRepo(shapeRepo) {
    assert(JPH::Factory::sInstance);

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

    mContext = std::make_unique<JPHPhysicsWorldContext>();
    mContext->init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints);
}

NewPhysicsWorld::~NewPhysicsWorld() {

    // Unregisters all types with the factory and cleans up the default material
    JPH::UnregisterTypes();

    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void NewPhysicsWorld::initializeJPH() {
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
}

int NewPhysicsWorld::stepSimulation(f32 deltaTime) {
    PROFILE_FUNCTION();

    // Fixed timestep
    constexpr f32 PHYSICS_TIMESTEP = 1.0f / 60.0f;

    // Step multiple times if needed
    f32 stepCountf;
    mTickTimeRemainder = modf((deltaTime + mTickTimeRemainder) / PHYSICS_TIMESTEP, &stepCountf);

    constexpr i32 MAX_COLLISION_STEPS_PER_FRAME = 2;
    const i32 collisionSteps = glm::min(static_cast<i32>(stepCountf), MAX_COLLISION_STEPS_PER_FRAME);

    // Step the world
    mContext->update(PHYSICS_TIMESTEP, collisionSteps);
    return collisionSteps;
}

void NewPhysicsWorld::updateTerrainBody(HeightmapPatch& patch) {
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
    // Remove old body
    if (patch.physBodyID != INVALID_PHYS_BODY_ID) {
        mContext->updateShape(patch.physBodyID, newShape.Get(), false, JPH::EActivation::DontActivate);
    }
    else {
        patch.physBodyID = createTerrainBody(f32v3(centerPos.x, centerPos.y, 0.0f), newShape.Get());
    }
}

PhysBodyID NewPhysicsWorld::createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents) {
    CollisionShapeID shapeId = mShapeRepo.getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.y);

    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, shapeId, JPH::EMotionType::Dynamic, PhysicsObjectLayer::Dynamic);
    createSettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::RotationZ;

    return createEntityBody(createSettings, ownerEntity, shapeId);
}

std::unique_ptr<JPH::Character> NewPhysicsWorld::createSimpleCharacter(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents) {
    ASSERT_GAME_THREAD();

    CollisionShapeID shapeId = mShapeRepo.getOrAddCapsuleCollisionShape(halfExtents.x, halfExtents.y);
    JPH::ShapeSettings& shapeSettings = mShapeRepo.getJoltShapeSettings(shapeId);
    JPH::ShapeRefC shape = shapeSettings.Create().Get();

    PhysBodyID standingShapeID = createCharacterCapsule(ownerEntity, position, halfExtents);

    JPH::CharacterSettings settings;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings.mLayer = e_cast(PhysicsObjectLayer::Dynamic);
    settings.mShape = shape;
    settings.mFriction = 0.5f;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisZ(), -halfExtents.y); // Accept contacts that touch the lower sphere of the capsule
    std::unique_ptr<JPH::Character> newCharacter = std::make_unique<JPH::Character>(
        &settings,
        JPH::RVec3::sZero(),
        JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f),
        PhysicsBodyUserData(ownerEntity),
        &mContext->getSystem()
    );
    newCharacter->AddToPhysicsSystem(JPH::EActivation::Activate);
    return newCharacter;
}

void NewPhysicsWorld::updateTileContainerMeshFromBuilder(StaticPhysicsMeshBuilder& meshBuilder) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();

    auto&& it = mTileContainerPhysicsData.find(meshBuilder.getOwnerTileContainerID());

    // Remove old collision
    if (!meshBuilder.hasAnyCollision()) {
        if (it != mTileContainerPhysicsData.end()) {
            for (auto& [key, physBodyID] : it->second->mTileKeyToPhysBodyID) {
                mContext->getBodyInterface().RemoveBody(JPH::BodyID(physBodyID));
            }
        }
        mTileContainerPhysicsData.erase(it);
        return;
    }

    if (it == mTileContainerPhysicsData.end()) {
        // Simply creating brand new collision
        NewTileContainerPhysicsData& newData = *mTileContainerPhysicsData.emplace(meshBuilder.getOwnerTileContainerID(), std::make_unique<NewTileContainerPhysicsData>()).first->second;
        addTrackedStaticRigidBodiesFromGatherer(meshBuilder.mTrackedRigidBodyGatherer, newData);
    }
    else {
        // May remove some old collision
        NewTileContainerPhysicsData& data = *it->second;
        panic("TODO: IMPLEMENT  NewPhysicsWorld::updateTileContainerMeshFromBuilder");
    }
}

void NewPhysicsWorld::removeBody(PhysBodyID id) {
    assert(id != INVALID_PHYS_BODY_ID);
    mContext->removeBody(id);
}

void NewPhysicsWorld::updateAndRenderImguiDebugControls() {

#ifdef JPH_DEBUG_RENDERER
    if (ImGui::Checkbox("Show Collision", &sDebugRenderer->mRenderSettings.showCollision)) {
        mContext->mDirtyStaticDebugRender = true;
    }

    if (sDebugRenderer->mRenderSettings.showCollision) {
        if (ImGui::Checkbox("Wireframe", &sDebugRenderer->mRenderSettings.wireframe)) {
            mContext->mDirtyStaticDebugRender = true;
        }

        if (ImGui::SliderFloat("Alpha", &sDebugRenderer->mRenderSettings.alpha, 0.0f, 1.0f)) {
            mContext->mDirtyStaticDebugRender = true;
        }
    }
#endif

    ImGui::Spacing();
    ImGui::SeparatorText("Stats");
    ImGui::Text(" Dynamic Bodies: %d", getBodyCount(PhysicsObjectLayer::Dynamic));
    ImGui::Text(" Static Bodies: %d", getBodyCount(PhysicsObjectLayer::Static));
}

void NewPhysicsWorld::debugRender(const Camera3D& camera) const {
#ifdef JPH_DEBUG_RENDERER
    PROFILE_FUNCTION();
    if (!sDebugRenderer) {
        sDebugRenderer = std::make_unique<PhysicsDebugRenderer>();
    }
    mContext->debugDraw(camera);
#endif //JPH_DEBUG_RENDERER
}

#if ENABLE_PHYSICS_ANALYTICS == 1
int NewPhysicsWorld::getBodyCount(PhysicsObjectLayer layer) const {
    return mContext->mBodyCounts[e_cast(layer)];
}
#endif

JPH::BodyCreationSettings NewPhysicsWorld::makeBodyCreateSettings(f32v3 position, const JPH::Shape* shape, JPH::EMotionType motionType, PhysicsObjectLayer layer) {
    // Adjust the position to account for the Z-up orientation
    JPH::RVec3 adjustedPosition(position.x, position.y, position.z/* + halfExtents.y*/);

    // Create a rotation quaternion to orient along the Z-axis
    static const JPH::Quat orientation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f);

    // Create the settings for the body itself
    return JPH::BodyCreationSettings(shape, adjustedPosition, orientation, motionType, e_cast(layer));
}

JPH::BodyCreationSettings NewPhysicsWorld::makeBodyCreateSettings(f32v3 position, CollisionShapeID shapeId, JPH::EMotionType motionType, PhysicsObjectLayer layer) {
    JPH::ShapeSettings& shapeSettings = mShapeRepo.getJoltShapeSettings(shapeId);
    JPH::ShapeRefC shape = shapeSettings.Create().Get();

    return makeBodyCreateSettings(position, shape, motionType, layer);
}

PhysBodyID NewPhysicsWorld::createEntityBody(const JPH::BodyCreationSettings& createSettings, entt::entity ownerEntity, CollisionShapeID shapeId) {
    ASSERT_GAME_THREAD();
   
    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::Activate, shapeId);
    body.SetUserData(PhysicsBodyUserData(ownerEntity));

    return body.GetID().GetIndexAndSequenceNumber();
}

PhysBodyID NewPhysicsWorld::createTileBody(TileContainerID containerId, TileIndex tileIndex, f32v3 position, CollisionShapeID shapeId) {
    ASSERT_GAME_THREAD();

    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, shapeId, JPH::EMotionType::Static, PhysicsObjectLayer::Static);

    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::DontActivate, shapeId);
    body.SetUserData(PhysicsBodyUserData(containerId, tileIndex));

    return body.GetID().GetIndexAndSequenceNumber();
}

PhysBodyID NewPhysicsWorld::createTerrainBody(f32v3 position, const JPH::Shape* terrainShape) {
    ASSERT_GAME_THREAD();
    JPH::BodyCreationSettings createSettings = makeBodyCreateSettings(position, terrainShape, JPH::EMotionType::Static, PhysicsObjectLayer::Static);

    JPH::Body& body = mContext->createBody(createSettings, JPH::EActivation::DontActivate, 696969 /*Cheeky terrain number for debug output*/);

    return body.GetID().GetIndexAndSequenceNumber();
}

void NewPhysicsWorld::addTrackedStaticRigidBodiesFromGatherer(TrackedStaticRigidBodyGatherer& gatherer, NewTileContainerPhysicsData& physicsData) {
    if (gatherer.mRigidBodiesToAdd.empty()) {
        return;
    }
    PROFILE_FUNCTION();

    for (auto& it : gatherer.mRigidBodiesToAdd) {
        const TileKey key = TileKey{ it.ownerTilePosition, it.tileId, it.layer };
        auto&& pit = physicsData.mTileKeyToPhysBodyID.find(key);
        // Only add if it doesn't already exist
        if (pit == physicsData.mTileKeyToPhysBodyID.end()) {
            physicsData.mTileKeyToPhysBodyID.emplace(key, createTileBody(gatherer.mContainerId, it.ownerTilePosition, it.position, it.shapeId));
        }
    }
}