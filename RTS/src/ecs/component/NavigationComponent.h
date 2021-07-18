#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;

enum class NavigationType {
	PATH,
	SIMPLE_LINEAR,
	INVALID
};

struct NavigationComponent {

	// Make sure navigation component is destroyed before the callback owner is destroyed
    // Callback should ideally only be set from the same entity
    void setSimpleLinearTargetPoint(const ui32v2& targetPoint, std::function<void(bool)> finishedCallback);
	void setPathWithCallback(std::unique_ptr<Path> path, std::function<void(bool)> finishedCallback);
	void abort();

	float mSpeed = 1.0f;
	union {
		struct {
			std::unique_ptr<Path> mPath;
			ui32 mCurrentPoint = 0;
		};
		ui32v2 mSimpleTargetPoint;
	};
	bool mColliding = false; // Colliding with another agent
	ui8 mFramesUntilNextRayCheck = 0;
	std::function<void(bool)> mFinishedCallback = nullptr;
	NavigationType mNavigationType = NavigationType::INVALID;
};

class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};