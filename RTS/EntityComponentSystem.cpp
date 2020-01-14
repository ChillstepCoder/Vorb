#include "stdafx.h"
#include "EntityComponentSystem.h"

const float ZERO_VELOCITY_EPSILON = 0.01f;

EntityComponentSystem::EntityComponentSystem(b2World& physWorld)
	: mPhysWorld(physWorld)
	, mPhysicsTable(physWorld) {
	addComponentTable(PhysicsComponentTable::NAME, &mPhysicsTable);
	addComponentTable(SimpleSpriteComponentTable::NAME, &mSpriteTable);
	addComponentTable(NavigationComponentTable::NAME, &mNavigationTable);
	addComponentTable(UndeadAIComponentTable::NAME, &mUndeadAITable);
}

void EntityComponentSystem::update(float deltaTime, TileGrid& world) {
	
	// TODO: Not every frame
	static int frameCount = 0;
	if (++frameCount % 4 == 0) {
		mUndeadAITable.update(*this, world);
	}
	mNavigationTable.update(*this);
	mPhysWorld.Step(deltaTime, 1, 1);
	//mPhysicsTable.update(deltaTime);
}
