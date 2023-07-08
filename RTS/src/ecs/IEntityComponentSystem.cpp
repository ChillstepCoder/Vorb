#include "stdafx.h"
#include "IEntityComponentSystem.h"

#include "world/IWorld.h"

#include "camera/Camera3D.h"

// TODO: Get rid of this
#include "rendering/RenderContext.h"

const float DEAD_COLOR_MULT = 0.4f;

IEntityComponentSystem::IEntityComponentSystem(IWorld& world) : mWorld(world) {
}

IEntityComponentSystem::~IEntityComponentSystem() {

}

void IEntityComponentSystem::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();
	
    //mPlayerControlSystem.update(mRegistry, playerCamera);
    // TODO: Move 
  
	mTimedTileInteractSystem.update(mRegistry);

	mFishingSystem.update(mWorld, mRegistry, elapsedSec);

    //mCorpseTable.update();
    
	// TODO: Client ECS
	if (RenderContext::exists()) {
		const Camera3D* camera = RenderContext::getInstance().getCamera();
		if (camera) {
			mPlayerControlSystem.update(mWorld, mRegistry, camera->getYaw());
		}
	}

    mSkillsSystem.update(mWorld, mRegistry, elapsedSec);

}

void IEntityComponentSystem::tickPhysics(f32 elapsedSec) {
	mPhysicsSystem.update(mWorld, mRegistry);
	mCharacterControlSystem.update(mRegistry);
}

void IEntityComponentSystem::setLocalPlayer(entt::entity playerEntity)
{
    ASSERT_GAME_THREAD();
	if (mPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);
    mPlayerEntity = playerEntity;
}
