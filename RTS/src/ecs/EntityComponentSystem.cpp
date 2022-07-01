#include "stdafx.h"
#include "EntityComponentSystem.h"

#include "World.h"

#include "camera/Camera3D.h"

const float DEAD_COLOR_MULT = 0.4f;

EntityComponentSystem::EntityComponentSystem(World& world)
	: mPersonAISystem(world)
	, mBusinessSystem(world)
	, mTimedTileInteractSystem(world)
    , mWorld(world) {
}

void EntityComponentSystem::tick() {
	
    mBusinessSystem.update(mRegistry);
    //mPlayerControlSystem.update(mRegistry, playerCamera);
	mPersonAISystem.update(mRegistry);
    mNavigationSystem.update(mRegistry, mWorld);
	mCharacterControlSystem.update(mRegistry);
	mTimedTileInteractSystem.update(mRegistry);
	//mCorpseTable.update();
}

void EntityComponentSystem::frameUpdate(const Camera3D& playerCamera)
{
	// Client ECS
    mPlayerControlSystem.update(mRegistry, playerCamera);
}
