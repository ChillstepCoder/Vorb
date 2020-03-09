#pragma once
#include <Vorb/ecs/ECS.h>
#include <Vorb/ecs/ComponentTable.hpp>

#include <box2d/b2_world.h>

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/UndeadAIComponent.h"
#include "ecs/component/PlayerControlComponent.h"
#include "ecs/component/CombatComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/SoldierAIComponent.h"

// TODO: ecs file?
#include "rendering/CharacterModel.h";


class EntityComponentSystem : public vecs::ECS {
public:
	EntityComponentSystem(b2World& physWorld);

	void update(float deltaTime, TileGrid& world);
	void convertEntityToCorpse(vecs::EntityID entity);

	DECL_COMPONENT_TABLE(mPhysicsTable, PhysicsComponent);
	DECL_COMPONENT_TABLE(mSpriteTable, SimpleSpriteComponent);
	DECL_COMPONENT_TABLE(mNavigationTable, NavigationComponent);
	DECL_COMPONENT_TABLE(mUndeadAITable, UndeadAIComponent);
	DECL_COMPONENT_TABLE(mSoldierAITable, SoldierAIComponent);
	DECL_COMPONENT_TABLE(mPlayerControlTable, PlayerControlComponent);
	DECL_COMPONENT_TABLE(mCombatTable, CombatComponent);
	DECL_COMPONENT_TABLE(mCorpseTable, CorpseComponent);
	DECL_COMPONENT_TABLE(mCharacterModelTable, CharacterModelComponent);

	b2World& getPhysWorld() { return mPhysWorld; }

	PhysicsComponentTable mPhysicsTable;
	SimpleSpriteComponentTable mSpriteTable;
	NavigationComponentTable mNavigationTable;
	UndeadAIComponentTable mUndeadAITable;
	SoldierAIComponentTable mSoldierAITable;
	PlayerControlComponentTable mPlayerControlTable;
	CombatComponentTable mCombatTable;
	CorpseComponentTable mCorpseTable;
	CharacterModelComponentTable mCharacterModelTable;

private:

	b2World& mPhysWorld;
};

