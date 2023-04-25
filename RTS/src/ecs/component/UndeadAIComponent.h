#pragma once
#include "actor/ActorTypes.h"

struct UndeadAIComponent {
	int mState = 0;
	float mAttackCooldown = 0.0f;
};

class UndeadAIComponentTable {
public:
	//void update(EntityComponentSystem& ecs);
};