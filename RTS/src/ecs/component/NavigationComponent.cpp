#include "stdafx.h"
#include "NavigationComponent.h"

#include "ecs/EntityComponentSystem.h"
#include "services/Services.h"

#include "DebugRenderer.h"

#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"

#include <glm/gtx/rotate_vector.hpp>

#include "World.h"

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;
constexpr float MIN_DISTANCE = 0.5f; // TODO: This used to be 0.9, extra large to account for steering to steer around obstacles
//constexpr int QUADRANTS = 5; //bad name

bool updateComponentSimpleLinear(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, LocomotionComponent& motionCmp, World& world) {
  
	const f32v2& offset = f32v2(navCmp.mSimpleTargetPoint) - physCmp.getXYPosition();
    const float distance2 = glm::length2(offset);
    if (distance2 <= SQ(MIN_DISTANCE)) {
		motionCmp.mDesiredMode = LocomotionMode::IDLE;
        return true;
    }

	// TODO: Allow variable pathing urgency
    motionCmp.mDesiredMode = LocomotionMode::SPRINT;

	motionCmp.mDesiredDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
	return false;
}

bool updateComponentFinePath(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, LocomotionComponent& motionCmp, World& world) {

	if (!navCmp.mFinePath->finishedGenerating.load()) {
		return false;
    }

    ui32 numPoints = navCmp.mFinePath->getNumPoints();

    if (navCmp.mCurrentFinePoint >= numPoints) {
		return true;
	}

	// TODO: do this conversion in the generator?
	const PathPoint* points = navCmp.mFinePath->getPoints();
	const ui32v2& nextTilePos = points[navCmp.mCurrentFinePoint].xy;
	f32v2 nextPoint = f32v2(nextTilePos) + f32v2(0.5f);
	// Adjust next target point position slightly towards next point to account for circle colliders in our path
	// so we can adequately steer around them
	// THIS BREAKS WALL STEERING THO :C
    /*if (navCmp.mCurrentPoint < navCmp.mPath->numPoints - 1) {
        f32v2 nextNextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint + 1]) + f32v2(0.5f);
        constexpr f32 TARGET_EASE = 0.05f;
        nextPoint += glm::normalize(nextNextPoint - nextPoint) + TARGET_EASE;
    }*/

	// Check for stuck on new tile
	const Tile* targetTile = world.tryGetTileAtWorldPos(nextTilePos);
	if (targetTile) {
        const TileCollider* collider = targetTile->tryGetColliderMainThread();
        f32 baseZ = targetTile->getBaseZPositionUncompressedMainThread();
		// TODO: Remove
		if (sDebugOptions.mShowPaths) {
			DebugRenderer::drawWireQuad(f32v3(nextTilePos.x, nextTilePos.y, baseZ), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 1.0f));
		}
        if (baseZ >= physCmp.getZPosition() + 0.1f /*1.1*/) {
            // Climb
			motionCmp.mDesiredMode = LocomotionMode::JUMPING;
        }
	}

	const f32v2& offset = nextPoint - physCmp.getXYPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 <= SQ(MIN_DISTANCE)) {
        ++navCmp.mCurrentFinePoint;
        if (navCmp.mCurrentFinePoint >= numPoints) {
			// Target reached
			navCmp.mFinePath = nullptr;
			if (navCmp.mNavigationType == NavigationType::FINE_PATH && navCmp.mFinishedCallback) {
				navCmp.mFinishedCallback(true /* success */);
				navCmp.mFinishedCallback = nullptr;
			}
			return true;
		}
		else {
			// Immediately raycheck each time we get to a new point
			navCmp.mFramesUntilNextRayCheck = 0;
			nextPoint = f32v2(points[navCmp.mCurrentFinePoint].xy) + f32v2(0.5f);
		}
	}

	motionCmp.mDesiredDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    // TODO: Allow variable pathing urgency
    motionCmp.mDesiredMode = LocomotionMode::SPRINT;
	    
	// Steer around obstacles and corners
	// Raycast forward to find a collision intersect
	if (navCmp.mFramesUntilNextRayCheck == 0) {
		constexpr f32 STEER_MULT = 1.5f;
		f32v2 steerVector = motionCmp.mDesiredDirection * STEER_MULT; //Look ahead
		IntersectionHit2D hit = world.tryGetRaycastIntersect2D(physCmp.getXYPosition(), physCmp.getXYPosition() + steerVector, physCmp.getZPosition());
		if (hit.didHit()) {
			// Something in the way!

			// Check if we need to climb
			const Tile* tile = world.tryGetTileAtWorldPos(hit.tilePos);
			if (tile) {
				const TileCollider* collider = tile->tryGetColliderMainThread();
				f32 baseZ = tile->getBaseZPositionUncompressedMainThread();
				if (collider && hit.tilePos == nextTilePos && baseZ > physCmp.getZPosition() && baseZ < physCmp.getZPosition() + 1.1f) {
					// Climb
					motionCmp.mDesiredMode = LocomotionMode::BEGIN_JUMP;
				}
				else {
					// Steer

					f32 angle = atan2(-hit.normal.y, -hit.normal.x) - atan2(steerVector.y, steerVector.x);
					// Large negative is positive
					if (angle < -M_PIF) {
						angle = M_2_PIF - angle;
					}

					constexpr float STEERING_ADJUST = DEG_TO_RAD(30.0f);
					if (angle > 0.0f) {
						motionCmp.mDesiredDirection = glm::rotate(motionCmp.mDesiredDirection, -STEERING_ADJUST);
						steerVector = motionCmp.mDesiredDirection * STEER_MULT;
					}
					else {
						motionCmp.mDesiredDirection = glm::rotate(motionCmp.mDesiredDirection, STEERING_ADJUST);
						steerVector = motionCmp.mDesiredDirection * STEER_MULT;
					}

					// Debug render
					if (sDebugOptions.mShowPaths) {
						DebugRenderer::drawVector(hit.position, hit.delta, color4(0.0f, 1.0f, 0.0f, 0.8f), 250);
						DebugRenderer::drawVector(hit.position, hit.normal, color4(0.0f, 1.0f, 1.0f, 0.8f), 250);
						DebugRenderer::drawVector(physCmp.getXYPosition(), steerVector, color4(1.0f, 0.0f, 0.0f, 0.8f), 250);
					}
				}
			}
		}
		else {
			// No hits so relax for a bit
			navCmp.mFramesUntilNextRayCheck = RAYCHECK_INTERVAL_FRAMES;
			//DebugRenderer::drawVector(physCmp.getXYPosition(), steerVector, color4(1.0f, 0.0f, 0.0f, 0.8f), 250);
		}
	}
	else {
		--navCmp.mFramesUntilNextRayCheck;
	}
		
	// TODO: Do we need this?
	physCmp.mDir = motionCmp.mDesiredDirection;
	return false;

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

void onPathingFinished(PhysicsComponent& physCmp, NavigationComponent& navCmp, LocomotionComponent& motionCmp) {
	// Target reached
	motionCmp.mDesiredMode = LocomotionMode::IDLE;
	navCmp.mFinePath = nullptr;
	navCmp.mCoarsePath = nullptr;
	if (navCmp.mFinishedCallback) {
		navCmp.mFinishedCallback(true /* success */);
		navCmp.mFinishedCallback = nullptr;
    }
    navCmp.mNavigationType = NavigationType::INVALID;
}

void requestPathToCoarsePoint(NavigationComponent& navCmp, PhysicsComponent& physCmp, f32v2 nextCoarseTilePos, World& world) {
    navCmp.mPendingFinePath = std::make_shared<NavPath>();
	if (sDebugOptions.mShowPaths) {
		// Make sure we dont free this path before it is rendered
		std::shared_ptr<NavPath> pathHandle = navCmp.mPendingFinePath;
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, PathPoint(physCmp.getXYPosition()), PathPoint(nextCoarseTilePos), false /*isCoarse*/, [pathHandle, &world]() {
			DebugRenderer::drawPath(*pathHandle, color4(1.0f, 0.0f, 1.0f), world.getWorldGrid(), 200);
		});
	}
	else {
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, PathPoint(physCmp.getXYPosition()), PathPoint(nextCoarseTilePos), false /*isCoarse*/);
	}
}

bool updateComponentCoarsePath(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, LocomotionComponent& motionCmp, World& world) {

    if (!navCmp.mCoarsePath->finishedGenerating.load()) {
        return false;
	}

	ui32 numPoints = navCmp.mCoarsePath->getNumPoints();

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
        return true;
    }

	const PathPoint* points = navCmp.mCoarsePath->getPoints();

	f32v2 nextCoarseTilePos = f32v2(points[navCmp.mCurrentCoarsePoint].xy) + f32v2(0.5f);

	const bool hasFinePath = navCmp.mPendingFinePath || navCmp.mFinePath;

	const f32v2& offset = nextCoarseTilePos - physCmp.getXYPosition();
    if (!hasFinePath) {
        // We need a path
        navCmp.mCurrentFinePoint = 0;
		requestPathToCoarsePoint(navCmp, physCmp, nextCoarseTilePos, world);

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
			if (updateComponentFinePath(entity, navCmp, physCmp, motionCmp, world)) {
                navCmp.mFinePath = nullptr;
				requestNextPath = true;
			}
			else if ((navCmp.mCurrentCoarsePoint < numPoints - 1) && (navCmp.mCurrentFinePoint > navCmp.mFinePath->getNumPoints() / 2u)) {
				// Halfway through path we start generating next path
				requestNextPath = true;
			}

			if (requestNextPath) {
                ++navCmp.mCurrentCoarsePoint;
                if (navCmp.mCurrentCoarsePoint >= numPoints) {
                    return true;
                }
                else {
                    // Immediately raycheck each time we get to a new point
                    navCmp.mFramesUntilNextRayCheck = 0;
					// Path forward
                    nextCoarseTilePos = f32v2(points[navCmp.mCurrentCoarsePoint].xy) + f32v2(0.5f);
                    requestPathToCoarsePoint(navCmp, physCmp, nextCoarseTilePos, world);
                }
			}
		}
		// TODO: Check LOS issues
    }

	return false;
}

void NavigationComponentSystem::update(entt::registry& registry, World& world) {
	// Update components
    auto view = registry.view<NavigationComponent, PhysicsComponent, LocomotionComponent>();

    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        auto& motionCmp = view.get<LocomotionComponent>(entity);
        switch (navCmp.mNavigationType) {
            case NavigationType::FINE_PATH:
				if (updateComponentFinePath(entity, navCmp, physCmp, motionCmp, world)) {
					onPathingFinished(physCmp, navCmp, motionCmp);
				}
                break;
            case NavigationType::COARSE_PATH:
				// TODO: Are these checks pointless?
				if (updateComponentCoarsePath(entity, navCmp, physCmp, motionCmp, world)) {
					onPathingFinished(physCmp, navCmp, motionCmp);
				}
                break;
            case NavigationType::SIMPLE_LINEAR:
                if (updateComponentSimpleLinear(entity, navCmp, physCmp, motionCmp, world)) {
                    onPathingFinished(physCmp, navCmp, motionCmp);
				}
                break;
            default:
				continue;
		}
		static_assert((int)NavigationType::INVALID == 3, "Update for new nav");
	}
}

void NavigationComponent::setSimpleLinearTargetPoint(const ui32v2& targetPoint, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::SIMPLE_LINEAR;
	mSimpleTargetPoint = targetPoint;
    mFinishedCallback = finishedCallback;
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);

    mCoarsePath.reset();
	mFinePath.reset();

	if (sDebugOptions.mShowPaths) {
		DebugRenderer::drawWireQuad(targetPoint, f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 0.8f), 50);
	}
}

void NavigationComponent::requestFinePath(const PathPoint& start, const PathPoint& goal) {
    requestCoarsePathWithCallback(start, goal, nullptr);
}

void NavigationComponent::requestCoarsePath(const PathPoint& start, const PathPoint& goal) {
	requestCoarsePathWithCallback(start, goal, nullptr);
}

void NavigationComponent::requestFinePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::FINE_PATH;
    mCoarsePath.reset();
    mFinePath = std::shared_ptr<NavPath>(new NavPath());
    Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, false /*isCoarse*/);
    mCurrentFinePoint = 0;
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mFinishedCallback = finishedCallback;
}

void NavigationComponent::requestCoarsePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
	Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, true /*isCoarse*/);
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    mFinishedCallback = finishedCallback;
}

void NavigationComponent::abort(LocomotionComponent& motionCmp) {
    motionCmp.mDesiredMode = LocomotionMode::IDLE;
    mFlags |= NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH;
	mFinePath.reset();
	if (mFinishedCallback) {
		mFinishedCallback(false /*success*/);
		mFinishedCallback = nullptr;
	}
}
