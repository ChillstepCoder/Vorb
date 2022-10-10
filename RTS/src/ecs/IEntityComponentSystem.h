#pragma once
#include "ecs/component/ComponentDefinition.h"
#include "util/StrToken.h"

class Camera3D;

class IEntityComponentSystem {
public:
    IEntityComponentSystem();
    virtual ~IEntityComponentSystem();

	void tick();
    void frameUpdate(const Camera3D& playerCamera);

    // Create an entity, on server it will optionally replicate, on client it cannot replicate
    virtual entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) = 0;
    // Client only, create an entity with a server entity mapping
    virtual entt::entity createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken) = 0;
    // Client or server
    virtual void destroyEntity(entt::entity entity) = 0;
    // Client only
    virtual void destroyEntityFromSrv(entt::entity entity) = 0;

    // TODO: UniquePtr for faster include
    CharacterControlSystem mCharacterControlSystem;
    PlayerControlSystem mPlayerControlSystem;
    PersonAISystem mPersonAISystem;
    NavigationComponentSystem mNavigationSystem;
    TimedTileInteractSystem mTimedTileInteractSystem;

    // City stuff
    BusinessSystem mBusinessSystem;

	// Classes with World access
	friend class PhysicsComponent;

    entt::entity mPlayerEntity = entt::null;
    entt::registry mRegistry;
};