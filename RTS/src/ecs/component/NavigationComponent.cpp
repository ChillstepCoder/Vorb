#include "stdafx.h"
#include "NavigationComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "debugging/DebugRenderer.h"

#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"

#include <glm/gtx/rotate_vector.hpp>

#include "world/IWorld.h"

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;
constexpr float MIN_DISTANCE = 0.5f; // TODO: This used to be 0.9, extra large to account for steering to steer around obstacles
//constexpr int QUADRANTS = 5; //bad name

bool updateComponentSimpleLinear(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {
  
	const f32v2& offset = f32v2(navCmp.mSimpleTargetPoint) - *(f32v2*)&pos;
    const float distance2 = glm::length2(offset);
    if (distance2 <= SQ(MIN_DISTANCE)) {
		motionCmp.mDesiredMode = LocomotionMode::IDLE;
        return true;
    }

	// TODO: Allow variable pathing urgency
    motionCmp.mDesiredMode = LocomotionMode::SPRINT;

	motionCmp.mMoveDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
	return false;
}

bool updateComponentFinePath(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {

	if (!navCmp.mFinePath->finishedGenerating.load()) {
		return false;
    }

    // Update tileNavData
	// TODO: Entity steering
    /*ui32v2 navCell = ui32v2(pos.x * 0.5f, pos.y * 0.5f);
    if (navCell != navCmp.mPrevNavCell) {
        assert(false);
    }*/

    ui32 numPoints = navCmp.mFinePath->getNumPoints();

    if (navCmp.mCurrentFinePoint >= numPoints) {
		return true;
	}

	// TODO: do this conversion in the generator?
	const LiteTileHandle* points = navCmp.mFinePath->getPoints();
	const i32v3 nextTilePos = points[navCmp.mCurrentFinePoint].getWorldPosition();
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
	const Tile* targetTile = sWorld->getTileHandleAtWorldPos(nextTilePos).tile;
	if (targetTile) {
        f32 baseZ = targetTile->getGroundZOffsetMainThread();
		// TODO: Remove
		if (sDebugOptions.mShowPaths) {
			DebugRenderer::drawWireQuad(f32v3(nextTilePos.x, nextTilePos.y, baseZ), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 1.0f));
		}
        if (baseZ >= pos.z + 0.1f /*1.1*/) {
            // Climb
			motionCmp.mDesiredMode = LocomotionMode::JUMPING;
        }
	}

	const f32v2& offset = nextPoint - f32v2(pos);
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
			nextPoint = f32v2(points[navCmp.mCurrentFinePoint].getWorldPosition()) + f32v2(0.5f);
		}
	}

	motionCmp.mMoveDirection = (offset / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    // TODO: Allow variable pathing urgency
    motionCmp.mDesiredMode = LocomotionMode::SPRINT;
	    
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

void onPathingFinished(NavigationComponent& navCmp, CharacterControlComponent& motionCmp) {
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

void requestFinePathToPoint(NavigationComponent& navCmp, const TileHandle& start, const TileHandle& goal) {
    navCmp.mPendingFinePath = std::make_shared<NavPath>();
	if (sDebugOptions.mShowPaths) {
		// Make sure we dont free this path before it is rendered
		std::shared_ptr<NavPath> pathHandle = navCmp.mPendingFinePath;
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/, [pathHandle]() {
			DebugRenderer::drawPath(*pathHandle, color4(1.0f, 0.0f, 1.0f), sWorld->getHeightmapGrid(), 200);
		});
	}
	else {
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/);
	}
}

bool updateComponentCoarsePath(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {

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

	const LiteTileHandle* points = navCmp.mCoarsePath->getPoints();

	f32v3 nextCoarseTilePos = f32v3(points[navCmp.mCurrentCoarsePoint].getWorldPosition()) + f32v3(0.5f, 0.5f, 0.0f);

	const bool hasFinePath = navCmp.mPendingFinePath || navCmp.mFinePath;

    if (!hasFinePath) {
        // We need a path
        navCmp.mCurrentFinePoint = 0;
		requestFinePathToPoint(navCmp, sWorld->getTileHandleAtWorldPos(pos), sWorld->getTileHandleAtWorldPos(nextCoarseTilePos));

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
			if (updateComponentFinePath(entity, navCmp, motionCmp, pos)) {
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
                    nextCoarseTilePos = f32v3(points[navCmp.mCurrentCoarsePoint].getWorldPosition()) + f32v3(0.5f, 0.5f, 0.0f);
					// TODO: REMOVE
					DebugRenderer::drawFilledQuad(f32v3(points[navCmp.mCurrentCoarsePoint].getWorldPosition()), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.8f), 10000);
                    requestFinePathToPoint(navCmp, sWorld->getTileHandleAtWorldPos(pos), sWorld->getTileHandleAtWorldPos(nextCoarseTilePos));
                }
			}
		}
		// TODO: Check LOS issues
    }

	return false;
}

bool updateComponentCoarsePathBuilding(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, const f32v3& pos) {
	assert(false);

	return false;
}

void NavigationComponentSystem::update(entt::registry& registry) {
	// Update components
    auto view = registry.view<NavigationComponent, PhysicsComponent, CharacterControlComponent>();


    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
		const f32v3 position = physCmp.getPosition();

        switch (navCmp.mNavigationType) {
            case NavigationType::FINE_PATH:
				if (updateComponentFinePath(entity, navCmp, controlCmp, position)) {
					onPathingFinished(navCmp, controlCmp);
				}
                break;
            case NavigationType::COARSE_PATH:
				// TODO: Are these checks pointless?
				if (updateComponentCoarsePath(entity, navCmp, controlCmp, position)) {
					onPathingFinished(navCmp, controlCmp);
				}
                break;
            case NavigationType::SIMPLE_LINEAR:
                if (updateComponentSimpleLinear(entity, navCmp, controlCmp, position)) {
                    onPathingFinished(navCmp, controlCmp);
				}
                break;
            case NavigationType::COARSE_BUILDING:
                if (updateComponentCoarsePathBuilding(entity, navCmp, controlCmp, position)) {
                    onPathingFinished(navCmp, controlCmp);
                }
                break;
            default:
				continue;
		}
		static_assert((int)NavigationType::INVALID == 4, "Update for new nav");
	}
}

void NavigationComponent::requestPathTo(const TileHandle& targetTile) {

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

void NavigationComponent::requestFinePath(const TileHandle& start, const TileHandle& goal) {
    requestCoarsePathWithCallback(start, goal, nullptr);
}

void NavigationComponent::requestCoarsePath(const TileHandle& start, const TileHandle& goal) {
	requestCoarsePathWithCallback(start, goal, nullptr);
}

void NavigationComponent::requestFinePathWithCallback(const TileHandle& start, const TileHandle& goal, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::FINE_PATH;
    mCoarsePath.reset();
    mFinePath = std::shared_ptr<NavPath>(new NavPath());
    Services::NavThread::ref().addPathfindTask(mFinePath, start, goal, false /*isCoarse*/);
    mCurrentFinePoint = 0;
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mFinishedCallback = finishedCallback;
}

void NavigationComponent::requestCoarsePathWithCallback(const TileHandle& start, const TileHandle& goal, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
	Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, true /*isCoarse*/);
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    mFinishedCallback = finishedCallback;
}


void NavigationComponent::abort(CharacterControlComponent& motionCmp) {
    motionCmp.mDesiredMode = LocomotionMode::IDLE;
    mFlags |= NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH;
	mFinePath.reset();
	if (mFinishedCallback) {
		mFinishedCallback(false /*success*/);
		mFinishedCallback = nullptr;
	}
}
