#pragma once

#include "actor/ActorTypes.h"
#include "pathfinding/PathFinder.h"
#include "tile/TileHarvestable.h"
#include "tile/TileHandle.h"

struct CharacterControlComponent;
class TileContainer;

enum class NavigationType : ui8 {
	FINE_PATH,
	COARSE_PATH,
	SIMPLE_LINEAR,
	INVALID
};
enum class NavigationStatus : ui8 {
	INVALID,
	IN_PROGRESS,
	SUCCESS,
	FAIL,
	COUNT,
};
enum class NavigationComponentFlags : ui8 {
    NAVIGATION_COMPONENT_FLAG_FAILED  = 1 << 0,
    NAVIGATION_COMPONENT_FLAG_SUCCESS = 1 << 1,
};
constexpr ui8 NAVIGATION_FINISHED_FLAGS = e_cast(NavigationComponentFlags::NAVIGATION_COMPONENT_FLAG_FAILED)
| e_cast(NavigationComponentFlags::NAVIGATION_COMPONENT_FLAG_SUCCESS);

struct NavigationComponent {

	// Make sure navigation component is destroyed before the callback owner is destroyed
    // Callback should ideally only be set from the same entity
    void setSimpleLinearTargetPoint(f32v3 targetPoint);

	void requestCoarsePath(f32v3 start, f32v3 goal);
	void requestCoarsePathToHarvestable(f32v3 start, TileHarvestable harvestable, f32 maxDistance);

	// TODO: RequestAbort so we dont need sharedptr?
	void abort(CharacterControlComponent& motionCmp);

	NavigationStatus getStatus() const { return mStatus; }

    // ============== Data ==============
	// TODO: I think we can make these unique_ptr/raw with an additional bool
	// TODO: Also we can compress these with a union once we no longer need smart pointer?
	// ~Actually... we cant properly abort paths if we dont use shared pointer because the nav thread needs copy of the path.
	// So what we do is instead store these as unique_ptr and abort is no longer instant, it is requestAbort which sets a flag, allowing us to abort only
	// when we are done with initial generation
    std::shared_ptr<NavPath> mFinePath;
    std::shared_ptr<NavPath> mPendingFinePath;
    std::shared_ptr<NavPath> mCoarsePath;
    ui32 mCurrentFinePoint = 0;
    ui32 mCurrentCoarsePoint = 0;
	f32v3 mTargetPosition = f32v3(0.0f);
	//ui32v2 mPrevNavCell; // TODO: for steering? Check steering each cell change?
    NavigationType mNavigationType = NavigationType::INVALID;
	NavigationStatus mStatus = NavigationStatus::INVALID;
	// TODO: Maybe this? Let the path find be more automatic?
	// TileRef mTargetTile; // Can be any tile in existance, automatically figures out how to nav to
	// void navigateTo(TileRef&& targetTile);
};

class NavigationComponentSystem {
public:
	void update(World& world, entt::registry& registry);
};