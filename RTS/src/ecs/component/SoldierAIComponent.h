#pragma once
#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;
class World;

struct SoldierAIComponent {
	int mState = 0;
	float mAttackCooldown = 0.0f;
	vecs::ComponentID mCombatComponent;
};

class SoldierAIComponentTable : public vecs::ComponentTable<SoldierAIComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs, World& world);
};