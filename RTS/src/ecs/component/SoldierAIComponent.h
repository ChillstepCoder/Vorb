#pragma once
#include "actor/ActorTypes.h"

class EntityComponentSystem;

struct SoldierAIComponent {
	int mState = 0;
	float mAttackCooldown = 0.0f;
};

class SoldierAIComponentTable {
public:
	void update(EntityComponentSystem& ecs);
};