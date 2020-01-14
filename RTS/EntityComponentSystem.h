#pragma once
#include <Vorb/ecs/ECS.h>
#include <Vorb/ecs/ComponentTable.hpp>

#include <box2d/b2_world.h>

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/UndeadAIComponent.h"
#include "ecs/component/PlayerControlComponent.h"


class EntityComponentSystem : public vecs::ECS {
public:
	EntityComponentSystem(b2World& physWorld);

	void update(float deltaTime, TileGrid& world);

	DECL_COMPONENT_TABLE(mPhysicsTable, PhysicsComponent);
	DECL_COMPONENT_TABLE(mSpriteTable, SimpleSpriteComponent);
	DECL_COMPONENT_TABLE(mNavigationTable, NavigationComponent);
	DECL_COMPONENT_TABLE(mUndeadAITable, UndeadAIComponent);
	DECL_COMPONENT_TABLE(mPlayerControlTable, PlayerControlComponent);

	b2World& getPhysWorld() { return mPhysWorld; }

	PhysicsComponentTable mPhysicsTable;
	SimpleSpriteComponentTable mSpriteTable;
	NavigationComponentTable mNavigationTable;
	UndeadAIComponentTable mUndeadAITable;
	PlayerControlComponentTable mPlayerControlTable;

private:
	b2World& mPhysWorld;
};

