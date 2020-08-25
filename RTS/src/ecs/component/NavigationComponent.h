#pragma once

#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;
class World;

struct NavigationComponent {

	// TODO Pathfinding
	// TODO Entity handles
	f32v2 mTargetPos = f32v2(0.0f);
	float mSpeed = 1.0f;
	bool mHasTarget = false;
	bool mColliding = false;
};

class NavigationComponentTable : public vecs::ComponentTable<NavigationComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs, World& world);
};