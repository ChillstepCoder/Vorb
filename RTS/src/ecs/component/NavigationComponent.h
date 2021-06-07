#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;

struct NavigationComponent {

	// Make sure navigation component is destroyed before the callback owner is destroyed
	// Callback should ideally only be set from the same entity
	void setPathWithCallback(std::unique_ptr<Path> path, std::function<void(bool)> finishedCallback);
	void abortPath();

	float mSpeed = 1.0f;
	std::unique_ptr<Path> mPath;
	ui32 mCurrentPoint = 0;
	bool mColliding = false; // Colliding with another agent
	ui8 mFramesUntilNextRayCheck = 0;
	std::function<void(bool)> mFinishedCallback = nullptr;
};

class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};