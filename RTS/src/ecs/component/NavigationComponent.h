#pragma once

#include "actor/ActorTypes.h"

class World;

struct NavigationComponent {

	// TODO Pathfinding
	// TODO Entity handles
	f32v2 mTargetPos = f32v2(0.0f);
	float mSpeed = 1.0f;
	bool mHasTarget = false;
	bool mColliding = false;
};

class NavigationComponentTable {
public:
	void update(entt::registry& registry, World& world);
};