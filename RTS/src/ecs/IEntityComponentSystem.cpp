#include "stdafx.h"
#include "IEntityComponentSystem.h"

#include "world/IWorld.h"

#include "camera/Camera3D.h"

const float DEAD_COLOR_MULT = 0.4f;

IEntityComponentSystem::IEntityComponentSystem()
	: mPlayerControlSystem()
	, mPersonAISystem()
	, mBusinessSystem()
	, mTimedTileInteractSystem() {
}

IEntityComponentSystem::~IEntityComponentSystem() {

}

void IEntityComponentSystem::tick() {
	
    mBusinessSystem.update(mRegistry);
    //mPlayerControlSystem.update(mRegistry, playerCamera);
	mPersonAISystem.update(mRegistry);
    mNavigationSystem.update(mRegistry);
	mCharacterControlSystem.update(mRegistry);
	mTimedTileInteractSystem.update(mRegistry);
	//mCorpseTable.update();
}

void IEntityComponentSystem::frameUpdate(const Camera3D& playerCamera)
{
	// Client ECS
    mPlayerControlSystem.update(mRegistry, playerCamera);
}

void IEntityComponentSystem::setLocalPlayer(entt::entity playerEntity)
{
	if (mPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);
    mPlayerEntity = playerEntity;
}
