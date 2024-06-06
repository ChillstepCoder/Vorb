#include "stdafx.h"
#include "NavigationComponent.h"

#include "ecs/IEntityComponentSystem.h"

#include "debugging/DebugRenderer.h"

#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"
#include "pathfinding/NavPath.h"

#include "rendering/RenderThreadTasks.h"

#include <glm/gtx/rotate_vector.hpp>

#include "world/World.h"

enum class PathStatus {
	IN_PROGRESS,
	SUCCESS,
	FAIL,
	COUNT
};
inline bool isPathStatusDone(PathStatus pathStatus) {
    return pathStatus > PathStatus::IN_PROGRESS;
    static_assert(e_cast(PathStatus::COUNT) == 3);
}

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;
constexpr float MIN_DISTANCE = 0.5f; // TODO: This used to be 0.9, extra large to account for steering to steer around obstacles
//constexpr int QUADRANTS = 5; //bad name

bool updateComponentSimpleLinear(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {
  
	const f32v2& offset = f32v2(navCmp.mSimpleTargetPoint) - *(f32v2*)&pos;
    const float distance2 = glm::length2(offset);
    if (distance2 <= SQ(MIN_DISTANCE)) {
		motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
        return true;
    }

	// TODO: Allow variable pathing urgency
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::SPRINT;

	motionCmp.mMoveDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
	return false;
}

PathStatus updateComponentFinePath(World& world, entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {

	if (!navCmp.mFinePath->finishedGenerating.load()) {
		return PathStatus::IN_PROGRESS;
	}
	else {
		navCmp.mTargetPosition = navCmp.mFinePath->getTargetPosition();
	}

    const ui32 numPoints = navCmp.mFinePath->getNumPoints();
	if (numPoints == 0) {
		return PathStatus::FAIL;
	}

    if (navCmp.mCurrentFinePoint >= numPoints) {
		return PathStatus::SUCCESS;
	}

	const NavPathPoint* points = navCmp.mFinePath->getPoints();
	const i32v3 nextTilePos = points[navCmp.mCurrentFinePoint].pos;
	f32v2 nextPoint = f32v2(nextTilePos) + f32v2(0.5f);
	// Adjust next target point position slightly towards next point to account for circle colliders in our path
	// so we can adequately steer around them
	// THIS BREAKS WALL STEERING THO :C
    /*if (navCmp.mCurrentPoint < navCmp.mPath->numPoints - 1) {
        f32v2 nextNextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint + 1]) + f32v2(0.5f);
        constexpr f32 TARGET_EASE = 0.05f;
        nextPoint += glm::normalize(nextNextPoint - nextPoint) + TARGET_EASE;
    }*/

	// Check for stuck on new tile/jump
	const TileHandle tileHandle = world.getTileHandleAtWorldPos(nextTilePos);
	if (tileHandle.isValid()) {
        const f32 baseZ = tileHandle.getTile().getGroundZOffset();
        if (baseZ >= pos.z + 0.1f /*1.1*/) {
            // Climb
			motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::JUMPING;
        }
	}

	const f32v2& offset = nextPoint - f32v2(pos);
	const float distance2 = glm::length2(offset);
	if (distance2 <= SQ(MIN_DISTANCE)) {
        ++navCmp.mCurrentFinePoint;
        if (navCmp.mCurrentFinePoint >= numPoints) {
			// Target reached
			navCmp.mFinePath = nullptr;
			return PathStatus::SUCCESS;
		}
		else {
			// Immediately raycheck each time we get to a new point
			navCmp.mFramesUntilNextRayCheck = 0;
			nextPoint = f32v2(points[navCmp.mCurrentFinePoint].pos) + f32v2(0.5f);
		}
	}

	motionCmp.mMoveDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    // TODO: Allow variable pathing urgency
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::SPRINT;
	    
	// Steer around obstacles and corners
	// Raycast forward to find a collision intersect
	if (navCmp.mFramesUntilNextRayCheck == 0) {
		// Old pre 3D physics steering
		//constexpr f32 STEER_MULT = 1.5f;
		//f32v2 steerVector = motionCmp.mDesiredDirection * STEER_MULT; //Look ahead
		//IntersectionHit2D hit = world.tryGetRaycastIntersect2D(physCmp.getXYPosition(), physCmp.getXYPosition() + steerVector, physCmp.getZPosition());
		//if (hit.didHit()) {
		//	// Something in the way!

		//	// Check if we need to climb
		//	const Tile* tile = world.tryGetTileAtWorldPos(hit.tilePos);
		//	if (tile) {
		//		const TileCollider* collider = tile->tryGetColliderMainThread();
		//		f32 baseZ = tile->getGroundZPositionUncompressedMainThread();
		//		if (collider && hit.tilePos == nextTilePos && baseZ > physCmp.getZPosition() && baseZ < physCmp.getZPosition() + 1.1f) {
		//			// Climb
		//			motionCmp.mDesiredMode = LocomotionMode::BEGIN_JUMP;
		//		}
		//		else {
		//			// Steer

		//			f32 angle = atan2(-hit.normal.y, -hit.normal.x) - atan2(steerVector.y, steerVector.x);
		//			// Large negative is positive
		//			if (angle < -M_PIF) {
		//				angle = M_2_PIF - angle;
		//			}

		//			constexpr float STEERING_ADJUST = DEG_TO_RAD(30.0f);
		//			if (angle > 0.0f) {
		//				motionCmp.mDesiredDirection = glm::rotate(motionCmp.mDesiredDirection, -STEERING_ADJUST);
		//				steerVector = motionCmp.mDesiredDirection * STEER_MULT;
		//			}
		//			else {
		//				motionCmp.mDesiredDirection = glm::rotate(motionCmp.mDesiredDirection, STEERING_ADJUST);
		//				steerVector = motionCmp.mDesiredDirection * STEER_MULT;
		//			}

		//			// Debug render
		//			if (sDebugOptions.mShowPaths) {
		//				DebugRenderer::drawVector(hit.position, hit.delta, color4(0.0f, 1.0f, 0.0f, 0.8f), 250);
		//				DebugRenderer::drawVector(hit.position, hit.normal, color4(0.0f, 1.0f, 1.0f, 0.8f), 250);
		//				DebugRenderer::drawVector(physCmp.getXYPosition(), steerVector, color4(1.0f, 0.0f, 0.0f, 0.8f), 250);
		//			}
		//		}
		//	}
		//}
		//else {
		//	// No hits so relax for a bit
		//	navCmp.mFramesUntilNextRayCheck = RAYCHECK_INTERVAL_FRAMES;
		//	//DebugRenderer::drawVector(physCmp.getXYPosition(), steerVector, color4(1.0f, 0.0f, 0.0f, 0.8f), 250);
		//}
	}
	else {
		--navCmp.mFramesUntilNextRayCheck;
	}
		
	// TODO: Do we need this?
	//assert(false);
	//physCmp.mDir = motionCmp.mDesiredDirection;
	return PathStatus::IN_PROGRESS;

    //const float ARC_LENGTH = DEG_TO_RAD(175.0f);
	//// Look for undead allies
	//std::vector<EntityDistSortKey> actors = world.queryActorsInArc(physCmp.getXYPosition(), 5.0f, targetDir, ARC_LENGTH, ACTORTYPE_UNDEAD, ACTORTYPE_NONE, true, QUADRANTS, entity);
	//float closest[5] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	//entt::entity closestEnt[QUADRANTS];
	//for (auto&& it : actors) {
	//	if (it.first.dist < closest[it.first.quadrant]) {
	//		closest[it.first.quadrant] = it.first.dist;
	//		closestEnt[it.first.quadrant] = it.second;
	//	}
	//}
	// const int sequence[QUADRANTS] = { 2, 1, 3, 0, 4 }; // QUADRANTS 5
	//// const int sequence[QUADRANTS] = { 1, 0, 3 }; // QUADRANTS 3
	//int best = 1;
	//float furthestDist = 0.0f;
	//for (int i : sequence) {
	//	// Find the biggest gap prioritizing center
	//	if (closest[i] > furthestDist) {
	//		furthestDist = closest[i];
	//		best = i;
	//	}
	//}
	//// Flow into our selected gap
	//// TODO: (remove branching?)
	//if (best != 1) {
	//	const float SEGMENT_LENGTH = ARC_LENGTH / QUADRANTS;
	//	const float angles[QUADRANTS] = { -2 * SEGMENT_LENGTH, -SEGMENT_LENGTH, 0, SEGMENT_LENGTH, 2 * SEGMENT_LENGTH }; // QUADRANTS 5
	//	// const float angles[QUADRANTS] = { -SEGMENT_LENGTH, 0, SEGMENT_LENGTH }; // QUADRANTS 2
	//	targetVelocity = glm::rotate(targetVelocity, angles[best]);
	//}

	// Disable friction while we are navigating
	//myPhysCmp.mFrictionEnabled = false;
	
}

void onPathingFinished(NavigationComponent& navCmp, CharacterControlComponent& motionCmp, bool success) {
	// Target reached
	motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
	motionCmp.mMoveDirection = f32v2(0.0f);
    if (success) {
		if (navCmp.mTargetPosition == f32v3(0.0f)) {
			if (navCmp.mFinePath) {
				navCmp.mTargetPosition = navCmp.mFinePath->getTargetPosition();
			}
			else if (navCmp.mCoarsePath) {
                navCmp.mTargetPosition = navCmp.mCoarsePath->getTargetPosition();
			}
		}
		assert(navCmp.mTargetPosition != f32v3(0.0f));
		navCmp.mStatus = NavigationStatus::SUCCESS;
	}
    else {
		navCmp.mStatus = NavigationStatus::FAIL;
    }
    navCmp.mFinePath = nullptr;
    navCmp.mCoarsePath = nullptr;
    navCmp.mNavigationType = NavigationType::INVALID;
}

void requestFinePathToPoint(World& world, NavigationComponent& navCmp, const f32v3& start, const f32v3& goal) {
    navCmp.mPendingFinePath = std::make_shared<NavPath>();
	if (sDebugOptions.mShowPaths) {
		// Make sure we dont free this path before it is rendered
        std::shared_ptr<NavPath> pathHandle = navCmp.mPendingFinePath;
        assert(Services::isUsingNav());
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/, [pathHandle, &world]() {

			std::vector<f32v3>* pointsHandle = new std::vector<f32v3>(std::move(pathHandle->convertToWorldPoints(world.getHeightmapGrid())));

			RenderThreadTasks::getInstance().addGenericTask([](RenderContext&, void* vPathHandle) {
				std::vector<f32v3>* pathHandle = static_cast<std::vector<f32v3>*>(vPathHandle);
				DebugRenderer::drawPath(*pathHandle, color4(1.0f, 0.0f, 1.0f), 200);
				delete pathHandle;
			}, pointsHandle);
		});
	}
    else {
        assert(Services::isUsingNav());
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/, nullptr);
	}
}

void updateComponentCoarsePath(World& world, entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {

    if (!navCmp.mCoarsePath->finishedGenerating.load()) {
        return;
	}

	const ui32 numPoints = navCmp.mCoarsePath->getNumPoints();
    if (numPoints == 0) {
        onPathingFinished(navCmp, motionCmp, false /*success*/);
        return;
	}

    // Lazy initialize the coarse point to a look-ahead position for better pathing
    if (navCmp.mCurrentCoarsePoint == 0) {
		if (numPoints > 2) {
			navCmp.mCurrentCoarsePoint = 2;
		}
        else if (numPoints > 0) {
            navCmp.mCurrentCoarsePoint = numPoints - 1;
		}
    }

    if (navCmp.mCurrentCoarsePoint >= numPoints) {
		onPathingFinished(navCmp, motionCmp, true /*success*/);
        return;
    }

	const NavPathPoint* points = navCmp.mCoarsePath->getPoints();

	f32v3 nextCoarseTilePos = f32v3(points[navCmp.mCurrentCoarsePoint].pos) + f32v3(0.5f, 0.5f, 0.0f);

	const bool hasFinePath = navCmp.mPendingFinePath || navCmp.mFinePath;

    if (!hasFinePath) {
        // We need a path
        DebugRenderer::drawWireQuadThreadSafe(nextCoarseTilePos, f32v2(1.0f), color::HotPink, 99999);
        navCmp.mCurrentFinePoint = 0;
		requestFinePathToPoint(world, navCmp, pos, nextCoarseTilePos);

    }
    else {
		// Our fine path finished generating
		if (navCmp.mPendingFinePath && navCmp.mPendingFinePath->finishedGenerating) {
			navCmp.mCurrentFinePoint = 0;
			navCmp.mFinePath = std::move(navCmp.mPendingFinePath);
			navCmp.mPendingFinePath.reset();
		}

		if (navCmp.mFinePath) {
			bool requestNextPath = false;
			assert(navCmp.mFinePath->finishedGenerating.load());
			const PathStatus fineStatus = updateComponentFinePath(world, entity, navCmp, motionCmp, pos);
			if (fineStatus == PathStatus::SUCCESS) {
                navCmp.mFinePath = nullptr;
				requestNextPath = true;
			}
			else if (fineStatus == PathStatus::FAIL) {
                onPathingFinished(navCmp, motionCmp, false /*success*/);
			}
			else if ((navCmp.mCurrentCoarsePoint < numPoints - 1) && (navCmp.mCurrentFinePoint > navCmp.mFinePath->getNumPoints() / 2u)) {
				// Halfway through path we start generating next path
				// TODO: This causes a bug where it does this forever, check logic ^
				requestNextPath = true;
			}

			if (requestNextPath) {
                ++navCmp.mCurrentCoarsePoint;
                if (navCmp.mCurrentCoarsePoint >= numPoints) {
                    onPathingFinished(navCmp, motionCmp, true /*success*/);
					return;
                }
                else {
                    // Immediately raycheck each time we get to a new point
                    navCmp.mFramesUntilNextRayCheck = 0;
					// Path forward
                    nextCoarseTilePos = f32v3(points[navCmp.mCurrentCoarsePoint].pos) + f32v3(0.5f, 0.5f, 0.0f);
                    requestFinePathToPoint(world, navCmp, pos, nextCoarseTilePos);
                }
			}
		}
		// TODO: Check LOS issues
    }
}

void updateComponentCoarsePathBuilding(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {
	assert(false);

	return;
}

void NavigationComponentSystem::update(World& world, entt::registry& registry) {
	// Update components
    auto view = registry.view<NavigationComponent, PositionComponent, CharacterControlComponent>();


    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        auto& posCmp = view.get<PositionComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
		const f32v3 position = posCmp.mPosition;

        switch (navCmp.mNavigationType) {
			case NavigationType::FINE_PATH: {
				const PathStatus fineStatus = updateComponentFinePath(world, entity, navCmp, controlCmp, position);
				if (isPathStatusDone(fineStatus)) {
					onPathingFinished(navCmp, controlCmp, fineStatus == PathStatus::SUCCESS ? true : false);
				}
				break;
			}
            case NavigationType::COARSE_PATH:
				// TODO: Are these checks pointless?
				updateComponentCoarsePath(world, entity, navCmp, controlCmp, position);
                break;
            case NavigationType::SIMPLE_LINEAR:
                if (updateComponentSimpleLinear(entity, navCmp, controlCmp, position)) {
                    onPathingFinished(navCmp, controlCmp, true /*success*/);
				}
                break;
            case NavigationType::COARSE_BUILDING:
				updateComponentCoarsePathBuilding(entity, navCmp, controlCmp, position);
                break;
            default:
				continue;
		}
		static_assert((int)NavigationType::INVALID == 4, "Update for new nav");
	}
}

void NavigationComponent::requestPathTo(const LiteTileHandle& targetTile) {
	assert(false);
}

void NavigationComponent::setSimpleLinearTargetPoint(const ui32v2& targetPoint) {
    mNavigationType = NavigationType::SIMPLE_LINEAR;
	mSimpleTargetPoint = targetPoint;
	mStatus = NavigationStatus::IN_PROGRESS;

    mCoarsePath.reset();
	mFinePath.reset();

	if (sDebugOptions.mShowPaths) {
		DebugRenderer::drawWireQuad(targetPoint, f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 0.8f), 50);
	}
}

void NavigationComponent::requestCoarsePath(const f32v3& start, const f32v3& goal) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
	mTargetPosition = f32v3(0.0f);
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
    assert(Services::isUsingNav());
	mStatus = NavigationStatus::IN_PROGRESS;
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, true /*isCoarse*/, nullptr);
}

void NavigationComponent::requestCoarsePathToHarvestable(const f32v3& start, TileHarvestable harvestable, f32 maxDistance) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
    mTargetPosition = f32v3(0.0f);
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
    assert(Services::isUsingNav());
	mStatus = NavigationStatus::IN_PROGRESS;
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    Services::NavThread::ref().addPathfindToHarvestableTask(mCoarsePath, start, harvestable, maxDistance, nullptr);
}

void NavigationComponent::abort(CharacterControlComponent& motionCmp) {
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
    mStatus = NavigationStatus::IN_PROGRESS;
    mTargetPosition = f32v3(0.0f);
	mFinePath.reset();
}
