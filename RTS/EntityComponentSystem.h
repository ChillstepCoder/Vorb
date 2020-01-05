#pragma once
#include <Vorb/ecs/ECS.h>
#include <Vorb/ecs/ComponentTable.hpp>

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/UndeadAIComponent.h"


class EntityComponentSystem : public vecs::ECS {
public:
	EntityComponentSystem();

	void update(float deltaTime, TileGrid& world);

	DECL_COMPONENT_TABLE(mPhysicsTable, PhysicsComponent);
	DECL_COMPONENT_TABLE(mSpriteTable, SimpleSpriteComponent);
	DECL_COMPONENT_TABLE(mNavigationTable, NavigationComponent);
	DECL_COMPONENT_TABLE(mUndeadAITable, UndeadAIComponent);

private:
	PhysicsComponentTable mPhysicsTable;
	SimpleSpriteComponentTable mSpriteTable;
	NavigationComponentTable mNavigationTable;
	UndeadAIComponentTable mUndeadAITable;
};

