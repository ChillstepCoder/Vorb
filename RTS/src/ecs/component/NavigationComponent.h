#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;

enum class NavigationType : ui8 {
	FINE_PATH,
	COARSE_PATH,
	SIMPLE_LINEAR,
	INVALID
};

enum NavigationComponentFlags : ui8 {
	NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT    = 1 << 0,
	NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH          = 1 << 1,
    NAVIGATION_COMPONENT_FLAG_WAITING_COARSE_PATH_GEN = 1 << 2,
    NAVIGATION_COMPONENT_FLAG_WAITING_FINE_PATH_GEN   = 1 << 3,
};

constexpr ui8 NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN = NAVIGATION_COMPONENT_FLAG_WAITING_FINE_PATH_GEN | NAVIGATION_COMPONENT_FLAG_WAITING_COARSE_PATH_GEN;

struct NavigationComponent {

	// Make sure navigation component is destroyed before the callback owner is destroyed
    // Callback should ideally only be set from the same entity
    void setSimpleLinearTargetPoint(const ui32v2& targetPoint, std::function<void(bool)> finishedCallback);

    void requestFinePath(const PathPoint& start, const PathPoint& goal);
	void requestCoarsePath(const PathPoint& start, const PathPoint& goal);

    void requestFinePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);
    void requestCoarsePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);

    bool isWaitingPathGen() const { return mFlags & NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN; }

	void abort();

    float mSpeed = 1.0f;
	union {
		struct {
            ui32 mCurrentFinePoint;
            ui32 mCurrentCoarsePoint;
		};
		ui32v2 mSimpleTargetPoint = ui32v2(0);
    };
    std::shared_ptr<NavPath> mFinePath;
    std::shared_ptr<NavPath> mCoarsePath;
    std::function<void(bool)> mFinishedCallback = nullptr;
    NavigationType mNavigationType = NavigationType::INVALID;
	ui8 mFlags = 0u;
	ui8 mFramesUntilNextRayCheck = 0;
};
static_assert(sizeof(NavigationComponent) == 120, "Keep components small");

class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};