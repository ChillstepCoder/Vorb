#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"

class World;
class Building;
struct CharacterControlComponent;

enum class NavigationType : ui8 {
	FINE_PATH,
	COARSE_PATH,
	SIMPLE_LINEAR,
	COARSE_BUILDING,
	INVALID
};

enum NavigationComponentFlags : ui8 {
	NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT    = 1 << 0,
	NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH          = 1 << 1,
};

struct NavigationComponent {

	// void navigateTo(TileRef&& targetTile);

	// Make sure navigation component is destroyed before the callback owner is destroyed
    // Callback should ideally only be set from the same entity
    void setSimpleLinearTargetPoint(const ui32v2& targetPoint, std::function<void(bool)> finishedCallback);

    void requestFinePath(const PathPoint& start, const PathPoint& goal);
	void requestCoarsePath(const PathPoint& start, const PathPoint& goal);
	void requestFineBuildingPath(const Building& building, TileIndex start, TileIndex goal);

    void requestFinePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);
    void requestCoarsePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback);
    void requestFineBuildingPathWithCallback(const Building& building, TileIndex start, TileIndex goal, std::function<void(bool)> finishedCallback);

	// TODO: RequestAbort so we dont need sharedptr?
	void abort(CharacterControlComponent& motionCmp);

    // ============== Data ==============
	// TODO: We can eliminate this data with polling, maybe that is better? We call update anyways...
    std::function<void(bool)> mFinishedCallback = nullptr; // This is 64 bytes :/
	// TODO: I think we can make these unique_ptr/raw with an additional bool
	// TODO: Also we can compress these with a union once we no longer need smart pointer?
	// ~Actually... we cant properly abort paths if we dont use shared pointer because the nav thread needs copy of the path.
	// So what we do is instead store these as unique_ptr and abort is no longer instant, it is requestAbort which sets a flag, allowing us to abort only
	// when we are done with initial generation
    std::shared_ptr<NavPath> mFinePath;
    std::shared_ptr<NavPath> mPendingFinePath;
    std::shared_ptr<NavPath> mCoarsePath;
	const Building* mBuilding = nullptr;
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
	// TODO: Maybe this? Let the path find be more automatic?
	// TileRef mTargetTile; // Can be any tile in existance, automatically figures out how to nav to
	// void navigateTo(TileRef&& targetTile);
};
static_assert(sizeof(NavigationComponent) == 136, "Keep components small");


class NavigationComponentSystem {
public:
	void update(entt::registry& registry, World& world);
};