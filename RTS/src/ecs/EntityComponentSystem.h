#pragma once
#include "ecs/component/ComponentDefinition.h"

class Camera3D;

class EntityComponentSystem {
public:
	EntityComponentSystem();

	void tick();
    void frameUpdate(const Camera3D& playerCamera);

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
    CharacterControlSystem mCharacterControlSystem;
    PlayerControlSystem mPlayerControlSystem;
    PersonAISystem mPersonAISystem;
    NavigationComponentSystem mNavigationSystem;
    TimedTileInteractSystem mTimedTileInteractSystem;

    // City stuff
    BusinessSystem mBusinessSystem;
	//UndeadAIComponentTable mUndeadAITable;
	//SoldierAIComponentTable mSoldierAITable;
	//CombatComponentTable mCombatTable;
	//CorpseComponentTable mCorpseTable;

	// Classes with World access
	friend class PhysicsComponent;

    entt::entity mPlayerEntity = entt::null;
    entt::registry mRegistry;
};
