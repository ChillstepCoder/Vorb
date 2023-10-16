#pragma once
#include "ecs/component/ComponentDefinition.h"

#include "ecs/component/FishingComponent.h"

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
    void setLocalPlayer(entt::entity playerEntity);

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

    entt::entity mPlayerEntity = entt::null;
};