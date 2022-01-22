#include "stdafx.h"
#include <glm/gtx/rotate_vector.hpp>
#include "NavigationComponent.h"

#include "ecs/EntityComponentSystem.h"
#include "services/Services.h"

#include "DebugRenderer.h"

#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"

#include <glm/gtx/rotate_vector.hpp>

#include "World.h"


constexpr float JUMP_VELOCITY = 0.15f; // Matches player control component

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;

constexpr float MIN_DISTANCE = 0.9f; // This is extra large to account for steering to steer around obstacles
constexpr float ACCELERATION = 0.013f;
//constexpr int QUADRANTS = 5; //bad name

inline void updateVelocity(const f32v2& targetVelocity, PhysicsComponent& physCmp) {
    f32v2 velocityOffset = targetVelocity - physCmp.getLinearVelocity();
    float velocityDist = glm::length(velocityOffset);
    // TODO: DELTATIME
    //myPhysCmp.mBody->ApplyForce(reinterpret_cast<const b2Vec2&>(targetVelocity * 0.025f), myPhysCmp.mBody->GetWorldCenter(), true);
    if (velocityDist <= ACCELERATION) {
        physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
    }
    else {
        const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(physCmp.mBody->GetLinearVelocity());
        velocityOffset = (velocityOffset / velocityDist) * ACCELERATION + currentLinearVelocity;
        physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
    }
}

bool updateComponentSimpleLinear(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, World& world) {
    const f32v2& offset = f32v2(navCmp.mSimpleTargetPoint) - physCmp.getXYPosition();
    const float distance2 = glm::length2(offset);
    if (distance2 <= SQ(MIN_DISTANCE)) {
        return true;
    }

    const f32v2 targetVelocity = (offset / std::sqrt(distance2)) * navCmp.mSpeed/* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    updateVelocity(targetVelocity, physCmp);
	return false;
}

bool updateComponentFinePath(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, World& world) {

	if (navCmp.mFinePath->finishedGenerating.load()) {
		navCmp.mFlags &= (~NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN); // We are not waiting
	}
	else {
		return false;
	}

	if (navCmp.mCurrentFinePoint >= navCmp.mFinePath->numPoints) {
		return true;
	}

	// TODO: do this conversion in the generator?
	const ui32v2 nextTilePos = navCmp.mFinePath->points[navCmp.mCurrentFinePoint];
	f32v2 nextPoint = f32v2(nextTilePos) + f32v2(0.5f);
	// Adjust next target point position slightly towards next point to account for circle colliders in our path
	// so we can adequately steer around them
	// THIS BREAKS WALL STEERING THO :C
    /*if (navCmp.mCurrentPoint < navCmp.mPath->numPoints - 1) {
        f32v2 nextNextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint + 1]) + f32v2(0.5f);
        constexpr f32 TARGET_EASE = 0.05f;
        nextPoint += glm::normalize(nextNextPoint - nextPoint) + TARGET_EASE;
    }*/

	const f32v2& offset = nextPoint - physCmp.getXYPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 <= SQ(MIN_DISTANCE)) {
        ++navCmp.mCurrentFinePoint;
        if (navCmp.mCurrentFinePoint >= navCmp.mFinePath->numPoints) {
			// Target reached
            physCmp.mFlags |= enum_cast(PhysicsComponentFlag::FRICTION_ENABLED);
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
			nextPoint = f32v2(navCmp.mFinePath->points[navCmp.mCurrentFinePoint]) + f32v2(0.5f);
		}
	}

	f32v2 targetVelocity = (offset / std::sqrt(distance2)) * navCmp.mSpeed/* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    f32v2 targetDir = glm::normalize(targetVelocity); // TODO: Get rid of normalize
	    
	// Steer around obstacles and corners
	// Raycast forward to find a collision intersect
	if (navCmp.mFramesUntilNextRayCheck == 0) {
		constexpr f32 STEER_MULT = 1.5f;
		f32v2 steerVector = targetVelocity * STEER_MULT; //Look ahead
		IntersectionHit2D hit = world.tryGetRaycastIntersect2D(physCmp.getXYPosition(), physCmp.getXYPosition() + steerVector, physCmp.getZPosition());
		if (hit.didHit()) {
			// Something in the way!

			// Check if we need to climb
			const Tile* tile = world.tryGetTileAtWorldPos(hit.tilePos);
			if (tile) {
				const TileCollider* collider = tile->tryGetCollider();
				f32 baseZ = tile->getBaseZPositionUncompressed();
				if (collider && hit.tilePos == nextTilePos && baseZ > physCmp.getZPosition() && baseZ < physCmp.getZPosition() + 1.1f) {
					// Climb
					physCmp.setZVelocity(JUMP_VELOCITY);
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
						targetVelocity = glm::rotate(targetVelocity, -STEERING_ADJUST);
						steerVector = targetVelocity * STEER_MULT;
						targetDir = glm::normalize(targetVelocity);
					}
					else {
						targetVelocity = glm::rotate(targetVelocity, STEERING_ADJUST);
						steerVector = targetVelocity * STEER_MULT;
						targetDir = glm::normalize(targetVelocity);
					}

					// Debug render
					DebugRenderer::drawVector(hit.position, hit.delta, color4(0.0f, 1.0f, 0.0f, 0.8f), 250);
					DebugRenderer::drawVector(hit.position, hit.normal, color4(0.0f, 1.0f, 1.0f, 0.8f), 250);
					DebugRenderer::drawVector(physCmp.getXYPosition(), steerVector, color4(1.0f, 0.0f, 0.0f, 0.8f), 250);
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
	physCmp.mDir = targetDir;

    updateVelocity(targetVelocity, physCmp);

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

void onPathingFinished(PhysicsComponent& physCmp, NavigationComponent& navCmp) {
	// Target reached
	physCmp.mFlags |= enum_cast(PhysicsComponentFlag::FRICTION_ENABLED);
	navCmp.mFinePath = nullptr;
	navCmp.mCoarsePath = nullptr;
	if (navCmp.mFinishedCallback) {
		navCmp.mFinishedCallback(true /* success */);
		navCmp.mFinishedCallback = nullptr;
    }
    navCmp.mNavigationType = NavigationType::INVALID;
}

bool updateComponentCoarsePath(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, World& world) {

    if (navCmp.mCoarsePath->finishedGenerating.load()) {
        navCmp.mFlags &= (~NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN); // We are not waiting
    }
    else {
        return false;
    }

    if (navCmp.mCurrentCoarsePoint >= navCmp.mCoarsePath->numPoints) {
        return true;
    }

	f32v2 nextCoarseTilePos= f32v2(navCmp.mCoarsePath->points[navCmp.mCurrentCoarsePoint]) + f32v2(0.5f);

	const f32v2& offset = nextCoarseTilePos - physCmp.getXYPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 > SQ(MIN_DISTANCE)) {
        if (!navCmp.mFinePath) {
            navCmp.mCurrentFinePoint = 0;
			navCmp.mFinePath = std::make_shared<NavPath>();
			Services::NavThread::ref().addPathfindTask(navCmp.mFinePath, ui32v2(physCmp.getXYPosition()), ui32v2(nextCoarseTilePos), false /*isCoarse*/);
            if (navCmp.mFinePath) {
                if (sDebugOptions.mShowPaths) {
                    DebugRenderer::drawPath(*navCmp.mFinePath, color4(1.0f, 0.0f, 1.0f), world.getWorldGrid(), 200);
                }
			}
			else {
				navCmp.mCoarsePath.reset();
				navCmp.mFlags |= NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH;
				return true;
			}
        }
    }
	// First time this runs, fine path will likely be null which will request a new fine path
	if (!navCmp.mFinePath || updateComponentFinePath(entity, navCmp, physCmp, world)) {
		navCmp.mFinePath = nullptr;

		++navCmp.mCurrentCoarsePoint;
		if (navCmp.mCurrentCoarsePoint >= navCmp.mCoarsePath->numPoints) {
			onPathingFinished(physCmp, navCmp);

			return true;
		}
		else {
			// Immediately raycheck each time we get to a new point
			navCmp.mFramesUntilNextRayCheck = 0;
			nextCoarseTilePos = f32v2(navCmp.mCoarsePath->points[navCmp.mCurrentCoarsePoint]) + f32v2(0.5f);
		}
	}

	return false;
}

void NavigationComponentSystem::update(entt::registry& registry, World& world) {
	// Update components
    auto view = registry.view<NavigationComponent, PhysicsComponent>();

    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        switch (navCmp.mNavigationType) {
            case NavigationType::FINE_PATH:
				if (updateComponentFinePath(entity, navCmp, physCmp, world)) {
					onPathingFinished(physCmp, navCmp);
				}
                break;
            case NavigationType::COARSE_PATH:
				// TODO: Are these checks pointless?
				if (updateComponentCoarsePath(entity, navCmp, physCmp, world)) {
					onPathingFinished(physCmp, navCmp);
				}
                break;
            case NavigationType::SIMPLE_LINEAR:
				if (updateComponentSimpleLinear(entity, navCmp, physCmp, world)) {
                    physCmp.mFlags |= enum_cast(PhysicsComponentFlag::FRICTION_ENABLED);
					if (navCmp.mFinishedCallback) {
						navCmp.mFinishedCallback(true /* success */);
						navCmp.mFinishedCallback = nullptr;
					}
					navCmp.mNavigationType = NavigationType::INVALID;
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

    DebugRenderer::drawWireQuad(targetPoint, f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 0.8f), 50);
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
    mFinePath = std::make_shared<NavPath>();
    Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, false /*isCoarse*/);
    mCurrentFinePoint = 0;
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mFinishedCallback = finishedCallback;

    // Waiting fine path
    mFlags = (mFlags & (~NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN)) | NAVIGATION_COMPONENT_FLAG_WAITING_FINE_PATH_GEN;
}

void NavigationComponent::requestCoarsePathWithCallback(const PathPoint& start, const PathPoint& goal, std::function<void(bool)> finishedCallback) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
    mCoarsePath = std::make_shared<NavPath>();
	Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, true /*isCoarse*/);
    mFlags &= (~NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH);
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0;
    mFinishedCallback = finishedCallback;

    // Waiting coarse path
    mFlags = (mFlags & (~NAVIGATION_COMPONENT_MASK_WAITING_PATH_GEN)) | NAVIGATION_COMPONENT_FLAG_WAITING_COARSE_PATH_GEN;
}

void NavigationComponent::abort() {
    mFlags |= NAVIGATION_COMPONENT_FLAG_FAILED_TO_PATH;
	mFinePath.reset();
	if (mFinishedCallback) {
		mFinishedCallback(false /*success*/);
		mFinishedCallback = nullptr;
	}
}
