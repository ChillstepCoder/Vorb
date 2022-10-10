#pragma once
#include "actor/ActorTypes.h"

class IEntityComponentSystem;

struct SoldierAIComponent {
	int mState = 0;
	float mAttackCooldown = 0.0f;
};

class SoldierAIComponentTable {
public:
	void update(IEntityComponentSystem& ecs);
};