#pragma once
#include "ecs/ClientEcsData.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/UndeadAIComponent.h"
#include "ecs/component/PlayerControlComponent.h"
#include "ecs/component/CombatComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/SoldierAIComponent.h"
#include "ecs/component/DynamicLightComponent.h"

// TODO: ecs file?
#include "rendering/CharacterModel.h"

class World;

class EntityComponentSystem {
public:
	EntityComponentSystem(World& world);

	void update(float deltaTime, const ClientECSData& clientData);
	void convertEntityToCorpse(entt::entity entity);

    /*DECL_COMPONENT_TABLE(mPhysicsTable, PhysicsComponent);
    DECL_COMPONENT_TABLE(mSpriteTable, SimpleSpriteComponent);
    DECL_COMPONENT_TABLE(mNavigationTable, NavigationComponent);
    DECL_COMPONENT_TABLE(mUndeadAITable, UndeadAIComponent);
    DECL_COMPONENT_TABLE(mSoldierAITable, SoldierAIComponent);
    DECL_COMPONENT_TABLE(mPlayerControlTable, PlayerControlComponent);
    DECL_COMPONENT_TABLE(mCombatTable, CombatComponent);
    DECL_COMPONENT_TABLE(mCorpseTable, CorpseComponent);
    DECL_COMPONENT_TABLE(mCharacterModelTable, CharacterModelComponent);
    DECL_COMPONENT_TABLE(mDynamicLightComponentTable, DynamicLightComponent);*/

	PhysicsSystem mPhysicsSystem;
	//NavigationComponentTable mNavigationTable;
	//UndeadAIComponentTable mUndeadAITable;
	//SoldierAIComponentTable mSoldierAITable;
	PlayerControlSystem mPlayerControlSystem;
	//CombatComponentTable mCombatTable;
	//CorpseComponentTable mCorpseTable;

	// Classes with World access
	friend class PhysicsComponent;

    entt::registry mRegistry;
	World& mWorld;
};
