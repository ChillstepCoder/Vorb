#include "stdafx.h"
#include "EntityComponentSystem.h"

const float ZERO_VELOCITY_EPSILON = 0.01f;

const std::string& SimpleSpriteComponentTable::NAME = "simplesprite";


EntityComponentSystem::EntityComponentSystem() {
	addComponentTable(PhysicsComponentTable::NAME, &mPhysicsTable);
	addComponentTable(SimpleSpriteComponentTable::NAME, &mSpriteTable);
}

void EntityComponentSystem::update(float deltaTime) {
	mPhysicsTable.update(deltaTime);
}
