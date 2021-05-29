#include "stdafx.h"
#include "EntityComponentSystem.h"

#include "World.h"

#include <box2d/b2_fixture.h>

const float DEAD_COLOR_MULT = 0.4f;

EntityComponentSystem::EntityComponentSystem(World& world)
	: mPhysicsSystem(world)
	, mPersonAISystem(world)
	, mBusinessSystem(world)
    , mWorld(world) {
}

void EntityComponentSystem::update(float deltaTime, const ClientECSData& clientData) {
	
	// TODO: Not every frame
	static int frameCount = 0;
	int frameMod = ++frameCount % 4;
    /*if (frameMod == 0) {
        mUndeadAITable.update(*this, mWorld);
    }
    else if (frameMod == 2) {
        mSoldierAITable.update(*this, mWorld);
    }
    mSpriteTable.update();*/
    mBusinessSystem.update(mRegistry, deltaTime);
	mPhysicsSystem.update(mRegistry, deltaTime); // Phys cmp sets dir to velocity
	//mNavigationTable.update(*this, mWorld); // Navigation sets dir to target
	mPlayerControlSystem.update(mRegistry, mWorld, clientData);
	mPersonAISystem.update(mRegistry, deltaTime);
	mNavigationSystem.update(mRegistry, mWorld, deltaTime);
	//mCorpseTable.update();
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
