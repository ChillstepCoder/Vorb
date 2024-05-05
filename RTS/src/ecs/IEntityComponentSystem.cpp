#include "stdafx.h"
#include "IEntityComponentSystem.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "camera/Camera3D.h"

// TODO: Get rid of this
#include "rendering/RenderContext.h"

const float DEAD_COLOR_MULT = 0.4f;

IEntityComponentSystem::IEntityComponentSystem(World& world) : mWorld(world) {
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

	ProjectileSystem::update(mWorld, mRegistry, elapsedSec);

}

void IEntityComponentSystem::tickPhysics(f32 elapsedSec) {
	mPhysicsSystem.update(mWorld, mRegistry);
	mCharacterControlSystem.update(mRegistry);
}

void IEntityComponentSystem::createFullEntitiesFromSimEntities(Chunk& chunk, const ChunkEntityFullActivateDataList& entities) {
	ASSERT_GAME_THREAD();
	const IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
	for (const EntityFullActivateData& activateData : entities) {
		// TODO: I dont think we need thread safe here as main thread is only writer?
		const f32 zPos = heightGrid.computeHeightAtPoint<true>(activateData.simPosition);
		entt::entity newEntity = entt::null;
		switch (activateData.entityType) {
			case SimEntityType::Person: {
				newEntity = createEntity(f32v3(activateData.simPosition.x, activateData.simPosition.y, zPos), CStrToken("villager"), true /*shouldReplicate*/);
				break;
			}
			case SimEntityType::Group:
			case SimEntityType::Settlement:
			default:
				panic("Tried to create invalid entity type {}", (int)activateData.entityType);
				break;

		}
		assert(activateData.binding);
        mRegistry.emplace<FullEntityBindingComponent>(newEntity).binding = activateData.binding;
		static_assert(e_count(SimEntityType) == 4);
	}
}

entt::entity IEntityComponentSystem::getLocalPlayerThreadSafe() const {
	std::lock_guard lock(mPlayerEntityMutex);
    return mPlayerEntity;
}

void IEntityComponentSystem::setLocalPlayer(entt::entity playerEntity)
{
    ASSERT_GAME_THREAD();
	if (mPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);

	std::lock_guard lock(mPlayerEntityMutex);
    mPlayerEntity = playerEntity;
}

f32v3 IEntityComponentSystem::getLocalPlayerPosition() {
	ASSERT_GAME_THREAD();
	entt::entity localPlayer = getLocalPlayer();
	if (localPlayer == entt::null) {
        return f32v3(0.0f);
    }
	return mRegistry.get<PositionComponent>(localPlayer).mPosition;
}
