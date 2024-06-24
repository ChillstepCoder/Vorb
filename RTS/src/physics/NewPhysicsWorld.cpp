#include "stdafx.h"
#include "NewPhysicsWorld.h"

// Jolt Documentation
// https://jrouwe.github.io/JoltPhysics/index.html
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>

// TODOS:
// 1. Custom heightfield collision shape https://jrouwe.github.io/JoltPhysics/index.html#creating-custom-shapes
// 2. Character and CharacterVirtual https://jrouwe.github.io/JoltPhysics/index.html#character-controllers
// 3. Convert to cpp20 module

class MyDebugRenderer : public JPH::DebugRenderer {
public:
    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override {
        throw std::logic_error("The method or operation is not implemented.");
    }


    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override {
        throw std::logic_error("The method or operation is not implemented.");
    }


    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override {
        throw std::logic_error("The method or operation is not implemented.");
    }


    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const ui32* inIndices, int inIndexCount) override {
        throw std::logic_error("The method or operation is not implemented.");
    }


    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override {
        throw std::logic_error("The method or operation is not implemented.");
    }


    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override {
        throw std::logic_error("The method or operation is not implemented.");
    }

};

#endif // JPH_DEBUG_RENDERER

// If you want your code to compile using single or double precision write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.
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

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace ObjectLayers {
    constexpr JPH::ObjectLayer Static(0);
    constexpr JPH::ObjectLayer Dynamic(1);
    constexpr JPH::ObjectLayer COUNT(2);
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case ObjectLayers::Static:
                return inObject2 == ObjectLayers::Dynamic; // Non moving only collides with moving
            case ObjectLayers::Dynamic:
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
        mObjectToBroadPhase[ObjectLayers::Static] = BroadPhaseLayers::Static;
        mObjectToBroadPhase[ObjectLayers::Dynamic] = BroadPhaseLayers::Dynamic;
    }

    virtual uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::Count;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < ObjectLayers::COUNT);
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
    JPH::BroadPhaseLayer mObjectToBroadPhase[ObjectLayers::COUNT];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case ObjectLayers::Static:
                return inLayer2 == BroadPhaseLayers::Dynamic;
            case ObjectLayers::Dynamic:
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
        return physicsSystem.GetBodyInterfaceNoLock();
    }

    void update(float deltaTime, int numCollisionSteps) {

        // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
        // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
        // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
        // TODO: Make smarter
        physicsSystem.OptimizeBroadPhase();

        physicsSystem.Update(deltaTime, numCollisionSteps, &tempAllocator, &jobSystem);
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

#ifdef JPH_DEBUG_RENDERER
    MyDebugRenderer debugRenderer;
#endif //JPH_DEBUG_RENDERER
};

NewPhysicsWorld::NewPhysicsWorld(World& world) : mWorld(world) {

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

    //// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
    //// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
    //JPH::BodyInterface& body_interface = mContext->getBodyInterface();

    //// Next we can create a rigid body to serve as the floor, we make a large box
    //// Create the settings for the collision volume (the shape).
    //// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
    //JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    //floor_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.

    //// Create the shape
    //JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    //JPH::ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

    //// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
    //JPH::BodyCreationSettings floor_settings(floor_shape, JPH::RVec3(0.0_r, -1.0_r, 0.0_r), JPH::Quat::sIdentity(), JPH::EMotionType::Static, ObjectLayers::Static);

    //// Create the actual rigid body
    //JPH::Body* floor = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

    //// Add it to the world
    //body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

    //// Now create a dynamic body to bounce on the floor
    //// Note that this uses the shorthand version of creating and adding a body to the world
    //JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0_r, 2.0_r, 0.0_r), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, ObjectLayers::Dynamic);
    //JPH::BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);

    //// Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    //// (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    //body_interface.SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));



    //// Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
    //body_interface.RemoveBody(sphere_id);

    //// Destroy the sphere. After this the sphere ID is no longer valid.
    //body_interface.DestroyBody(sphere_id);

    //// Remove and destroy the floor
    //body_interface.RemoveBody(floor->GetID());
    //body_interface.DestroyBody(floor->GetID());

}

NewPhysicsWorld::~NewPhysicsWorld() {

    // Unregisters all types with the factory and cleans up the default material
    JPH::UnregisterTypes();

    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

int NewPhysicsWorld::stepSimulation(f32 deltaTime) {

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

PhysBodyID NewPhysicsWorld::createCharacterCapsule(entt::entity ownerEntity, f32v3 position, f32v2 halfExtents) {

    JPH::BodyInterface& bodyInterface = mContext->getBodyInterface();

    // TODO: Cache shape settings
    JPH::CapsuleShapeSettings shapeSettings(halfExtents.y, halfExtents.x);

    JPH::ShapeSettings::ShapeResult capsuleShapeResult = shapeSettings.Create();
    JPH::ShapeRefC capsuleShape = capsuleShapeResult.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

    // Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
    JPH::BodyCreationSettings createSettings(capsuleShape, JPH::RVec3(position.x, position.y, position.z + halfExtents.y), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, ObjectLayers::Dynamic);

    // Create the actual rigid body
    JPH::Body* body = bodyInterface.CreateBody(createSettings); // Note that if we run out of bodies this can return nullptr
    const JPH::BodyID bodyID = body->GetID();
    // Add it to the world
    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);
    return bodyID.GetIndexAndSequenceNumber();
}
