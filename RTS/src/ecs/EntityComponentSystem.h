#pragma once
#include "ecs/ClientEcsData.h"
#include "ecs/component/ComponentDefinition.h"

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

    // TODO: UniquePtr for faster include
    PhysicsSystem mPhysicsSystem;
    PlayerControlSystem mPlayerControlSystem;
    PersonAISystem mPersonAISystem;
    NavigationComponentSystem mNavigationSystem;
	//UndeadAIComponentTable mUndeadAITable;
	//SoldierAIComponentTable mSoldierAITable;
	//CombatComponentTable mCombatTable;
	//CorpseComponentTable mCorpseTable;

	// Classes with World access
	friend class PhysicsComponent;

    entt::registry mRegistry;
	World& mWorld;
};
