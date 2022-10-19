#include "stdafx.h"
#include "IEntityComponentSystem.h"

#include "world/IWorld.h"

#include "camera/Camera3D.h"

// TODO: Get rid of this
#include "rendering/RenderContext.h"

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
    
	// TODO: Client ECS
	if (RenderContext::exists()) {
		const Camera3D* camera = RenderContext::getInstance().getCamera();
		if (camera) {
			mPlayerControlSystem.update(mRegistry, camera->getYaw());
		}
	}
}

void IEntityComponentSystem::setLocalPlayer(entt::entity playerEntity)
{
	if (mPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);
    mPlayerEntity = playerEntity;
}
