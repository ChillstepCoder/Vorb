#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;
struct CharacterControlComponent;

enum class NavigationType : ui8 {
	FINE_PATH,
	COARSE_PATH,
	SIMPLE_LINEAR,
	INVALID
};

enum NavigationComponentFlags : ui8 {
	NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT    = 1 << 0,
	NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH          = 1 << 1,
};

struct NavigationComponent {

	// Make sure navigation component is destroyed before the callback owner is destroyed
    // Callback should ideally only be set from the same entity
    void setSimpleLinearTargetPoint(const ui32v2& targetPoint, std::function<void(bool)> finishedCallback);

    void requestFinePath(const PathPoint& start, const PathPoint& goal);
	void requestCoarsePath(const PathPoint& start, const PathPoint& goal);

    void requestFinePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);
    void requestCoarsePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);

	void abort(CharacterControlComponent& motionCmp);

    // ============== Data ==============
    std::function<void(bool)> mFinishedCallback = nullptr;
	// TODO: I think we can make these unique_ptr with an additional bool
    std::shared_ptr<NavPath> mFinePath;
    std::shared_ptr<NavPath> mPendingFinePath;
    std::shared_ptr<NavPath> mCoarsePath;
	union {
		struct {
            ui32 mCurrentFinePoint;
            ui32 mCurrentCoarsePoint;
		};
		ui32v2 mSimpleTargetPoint = ui32v2(0);
    };
    NavigationType mNavigationType = NavigationType::INVALID;
	ui8 mFlags = 0u;
	ui8 mFramesUntilNextRayCheck = 0;
};
static_assert(sizeof(NavigationComponent) == 128, "Keep components small");

class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};