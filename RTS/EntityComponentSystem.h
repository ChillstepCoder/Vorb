#pragma once
#include <Vorb/ecs/ECS.h>
#include <Vorb/ecs/ComponentTable.hpp>

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"


class EntityComponentSystem : public vecs::ECS {
public:
	EntityComponentSystem();

	void update(float deltaTime);

	DECL_COMPONENT_TABLE(mPhysicsTable, PhysicsComponent);
	DECL_COMPONENT_TABLE(mSpriteTable, SimpleSpriteComponent);

private:
	PhysicsComponentTable mPhysicsTable;
	SimpleSpriteComponentTable mSpriteTable;
};

