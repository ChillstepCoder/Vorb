#include "stdafx.h"
#include "EntityComponentSystem.h"

#include "World.h"

#include <box2d/b2_fixture.h>
#include <Vorb/ecs/ComponentTable.hpp>

const float DEAD_COLOR_MULT = 0.4f;

EntityComponentSystem::EntityComponentSystem(World& world)
	: mPhysicsTable(world)
    , mWorld(world) {
	addComponentTable(PhysicsComponentTable::NAME, &mPhysicsTable);
	addComponentTable(SimpleSpriteComponentTable::NAME, &mSpriteTable);
	addComponentTable(NavigationComponentTable::NAME, &mNavigationTable);
	addComponentTable(UndeadAIComponentTable::NAME, &mUndeadAITable);
	addComponentTable(SoldierAIComponentTable::NAME, &mSoldierAITable);
	addComponentTable(PlayerControlComponentTable::NAME, &mPlayerControlTable);
	addComponentTable(CombatComponentTable::NAME, &mCombatTable);
	addComponentTable(CorpseComponentTable::NAME, &mCorpseTable);
    addComponentTable(CharacterModelComponentTable::NAME, &mCharacterModelTable);
    addComponentTable(DynamicLightComponentTable::NAME, &mDynamicLightComponentTable);
}

void EntityComponentSystem::update(float deltaTime, const ClientECSData& clientData) {
	
	// TODO: Not every frame
	static int frameCount = 0;
	int frameMod = ++frameCount % 4;
	if (frameMod == 0) {
		mUndeadAITable.update(*this, mWorld);
	}
	else if (frameMod == 2) {
		mSoldierAITable.update(*this, mWorld);
	}
	mSpriteTable.update();
	mPhysicsTable.update(deltaTime); // Phys cmp sets dir to velocity
	mNavigationTable.update(*this, mWorld); // Navigation sets dir to target
	mPlayerControlTable.update(*this, mWorld, clientData);
	mCorpseTable.update();
}

void EntityComponentSystem::convertEntityToCorpse(vecs::EntityID entity) {
	SimpleSpriteComponent& spriteComp = getSimpleSpriteComponentFromEntity(entity);
	spriteComp.color.r = ui8((float)spriteComp.color.r * DEAD_COLOR_MULT);
	spriteComp.color.g = ui8((float)spriteComp.color.g * DEAD_COLOR_MULT);
	spriteComp.color.b = ui8((float)spriteComp.color.b * DEAD_COLOR_MULT);

	// Change filter for no collide
	PhysicsComponent& physComp = getPhysicsComponentFromEntity(entity);
	physComp.mQueryActorTypes |= ACTORTYPE_CORPSE;
	b2Filter deadFilter;
	deadFilter.groupIndex = -1;
	physComp.mBody->GetFixtureList()->SetFilterData(deadFilter);

	UndeadAIComponent& undeadAiComp = getUndeadAIComponentFromEntity(entity);
	if (&undeadAiComp != &mUndeadAITable.getDefaultData()) {
		deleteComponentInternal(&mUndeadAITable, entity);
	}

	SoldierAIComponent& soldierAiComp = getSoldierAIComponentFromEntity(entity);
	if (&soldierAiComp != &mSoldierAITable.getDefaultData()) {
		deleteComponentInternal(&mSoldierAITable, entity);
	}
}
