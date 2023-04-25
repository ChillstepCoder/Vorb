#pragma once
#include "ecs/component/ComponentDefinition.h"
#include "util/StrToken.h"

class IEntityComponentSystem {
public:
    IEntityComponentSystem();
    virtual ~IEntityComponentSystem();

    virtual void tick();
    virtual void tickPhysics();

    // Create an entity, on server it will optionally replicate, on client it cannot replicate
    virtual entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) = 0;
    // Client or server
    virtual void destroyEntity(entt::entity entity) = 0;

    entt::entity getLocalPlayer() const { assert(IS_GAME_THREAD()); return mPlayerEntity; }
    void setLocalPlayer(entt::entity playerEntity);

    // TODO: UniquePtr for faster include
    CharacterControlSystem mCharacterControlSystem;
    PlayerControlSystem mPlayerControlSystem;
    TimedTileInteractSystem mTimedTileInteractSystem;
    PhysicsSystem mPhysicsSystem;

	// Classes with World access
	friend class PhysicsComponent;


    entt::registry mRegistry;

protected:

    entt::entity mPlayerEntity = entt::null;
};