#include "stdafx.h"
#include "EntityComponentSystem.h"

#include "World.h"

#include "camera/Camera3D.h"

const float DEAD_COLOR_MULT = 0.4f;

EntityComponentSystem::EntityComponentSystem(World& world)
	: mPhysicsSystem(world)
	, mPersonAISystem(world)
	, mBusinessSystem(world)
	, mTimedTileInteractSystem(world)
    , mWorld(world) {
}

void EntityComponentSystem::tick() {
	
    mBusinessSystem.update(mRegistry);
    //mPlayerControlSystem.update(mRegistry, playerCamera);
	mPersonAISystem.update(mRegistry);
    mNavigationSystem.update(mRegistry, mWorld);
	mLocomotionSystem.update(mRegistry);
    mPhysicsSystem.update(mRegistry); // Phys cmp sets dir to velocity
	mTimedTileInteractSystem.update(mRegistry);
	//mCorpseTable.update();
}

void EntityComponentSystem::frameUpdate(const Camera3D& playerCamera)
{
	// Client ECS
    mPlayerControlSystem.update(mRegistry, playerCamera);
}

void EntityComponentSystem::convertEntityToCorpse(entt::entity entity) {
	//SimpleSpriteComponent& spriteComp = mRegistry.get<SimpleSpriteComponent>(entity);
	//spriteComp.mColor.r = ui8((float)spriteComp.mColor.r * DEAD_COLOR_MULT);
	//spriteComp.mColor.g = ui8((float)spriteComp.mColor.g * DEAD_COLOR_MULT);
	//spriteComp.mColor.b = ui8((float)spriteComp.mColor.b * DEAD_COLOR_MULT);

	//// Change filter for no collide
	//PhysicsComponent& physComp = getPhysicsComponentFromEntity(entity);
	//physComp.mQueryActorTypes |= ACTORTYPE_CORPSE;
	//b2Filter deadFilter;
	//deadFilter.groupIndex = -1;
	//physComp.mBody->GetFixtureList()->SetFilterData(deadFilter);

	//UndeadAIComponent& undeadAiComp = getUndeadAIComponentFromEntity(entity);
	//if (&undeadAiComp != &mUndeadAITable.getDefaultData()) {
	//	deleteComponentInternal(&mUndeadAITable, entity);
	//}

	//SoldierAIComponent& soldierAiComp = getSoldierAIComponentFromEntity(entity);
	//if (&soldierAiComp != &mSoldierAITable.getDefaultData()) {
	//	deleteComponentInternal(&mSoldierAITable, entity);
	//}
}
