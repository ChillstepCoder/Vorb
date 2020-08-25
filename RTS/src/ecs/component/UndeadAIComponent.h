#pragma once
#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;
class World;

struct UndeadAIComponent {
	int mState = 0;
	float mAttackCooldown = 0.0f;
	vecs::ComponentID mCombatComponent;
};

class UndeadAIComponentTable : public vecs::ComponentTable<UndeadAIComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs, World& world);
};