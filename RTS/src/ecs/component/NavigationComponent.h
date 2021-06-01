#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;

struct NavigationComponent {
	float mSpeed = 1.0f;
	std::unique_ptr<Path> mPath;
	ui32 mCurrentPoint = 0;
	bool mColliding = false; // Colliding with another agent
	ui8 mFramesUntilNextRayCheck = 0;
};

class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};