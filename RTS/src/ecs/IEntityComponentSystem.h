#pragma once
#include "ecs/component/ComponentDefinition.h"

#include "ecs/component/FishingComponent.h"

#include <mutex>

class World;

class IEntityComponentSystem {
public:
    IEntityComponentSystem(World& world);
    virtual ~IEntityComponentSystem();

    virtual void tick(f32 elapsedSec);
    virtual void tickPhysics(f32 elapsedSec);

    // Create an entity, on server it will optionally replicate, on client it cannot replicate
    virtual entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) = 0;
    // Client or server
    virtual void destroyEntity(entt::entity entity) = 0;

    entt::entity getLocalPlayer() const { ASSERT_GAME_THREAD(); return mPlayerEntity; }
    entt::entity getLocalPlayerThreadSafe() const;
    void setLocalPlayer(entt::entity playerEntity);
    f32v3 getLocalPlayerPosition();

    // TODO: UniquePtr for faster include
    CharacterControlSystem mCharacterControlSystem;
    PlayerControlSystem mPlayerControlSystem;
    TimedTileInteractSystem mTimedTileInteractSystem;
    PhysicsSystem mPhysicsSystem;
    CameraAttachSystem mCameraAttachSystem;
    FishingComponentSystem mFishingSystem;
    SkillsComponentSystem mSkillsSystem;

	// Classes with World access
	friend class PhysicsComponent;

    World& mWorld;

    entt::registry mRegistry;

protected:
    mutable std::mutex mPlayerEntityMutex;
    entt::entity mPlayerEntity = entt::null;
};