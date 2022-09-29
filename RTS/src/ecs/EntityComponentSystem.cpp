#include "stdafx.h"
#include "EntityComponentSystem.h"

#include "world/IWorld.h"

#include "camera/Camera3D.h"

const float DEAD_COLOR_MULT = 0.4f;

EntityComponentSystem::EntityComponentSystem()
	: mPlayerControlSystem()
	, mPersonAISystem()
	, mBusinessSystem()
	, mTimedTileInteractSystem() {
}

void EntityComponentSystem::tick() {
	
    mBusinessSystem.update(mRegistry);
    //mPlayerControlSystem.update(mRegistry, playerCamera);
	mPersonAISystem.update(mRegistry);
    mNavigationSystem.update(mRegistry);
	mCharacterControlSystem.update(mRegistry);
	mTimedTileInteractSystem.update(mRegistry);
	//mCorpseTable.update();
}

void EntityComponentSystem::frameUpdate(const Camera3D& playerCamera)
{
	// Client ECS
    mPlayerControlSystem.update(mRegistry, playerCamera);
}
