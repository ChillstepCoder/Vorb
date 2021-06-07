#include "stdafx.h"
#include <glm/gtx/rotate_vector.hpp>
#include "NavigationComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "DebugRenderer.h"

#include <glm/gtx/rotate_vector.hpp>

#include "World.h"

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;

constexpr float MIN_DISTANCE = 0.9f; // This is extra large to account for steering to steer around obstacles
constexpr float ACCELERATION = 0.013f;
//constexpr int QUADRANTS = 5; //bad name

inline void updateComponent(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, World& world) {
	
	// TODO: do this conversion in the generator?
	f32v2 nextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint]) + f32v2(0.5f);
	// Adjust next target point position slightly towards next point to account for circle colliders in our path
	// so we can adequately steer around them
	if (navCmp.mCurrentPoint < navCmp.mPath->numPoints - 1) {
		f32v2 nextNextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint + 1]) + f32v2(0.5f);
		constexpr f32 TARGET_EASE = 0.05f;
		nextPoint += glm::normalize(nextNextPoint - nextPoint) + TARGET_EASE;
	}

	const f32v2& offset = nextPoint - physCmp.getXYPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 <= SQ(MIN_DISTANCE)) {
        ++navCmp.mCurrentPoint;
        if (navCmp.mCurrentPoint >= navCmp.mPath->numPoints) {
			// Target reached
            physCmp.mFlags |= enum_cast(PhysicsComponentFlag::FRICTION_ENABLED);
			navCmp.mPath = nullptr;
			if (navCmp.mFinishedCallback) {
				navCmp.mFinishedCallback(true /* success */);
				navCmp.mFinishedCallback = nullptr;
			}
			return;
		}
		else {
			// Immediately raycheck each time we get to a new point
			navCmp.mFramesUntilNextRayCheck = 0;
			nextPoint = f32v2(navCmp.mPath->points[navCmp.mCurrentPoint]) + f32v2(0.5f);
		}
	}

	f32v2 targetVelocity = (offset / std::sqrt(distance2)) * navCmp.mSpeed/* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    f32v2 targetDir = glm::normalize(targetVelocity); // TODO: Get rid of normalize
	    
	// Steer around obstacles and corners
	// Raycast forward to find a collision intersect
	if (navCmp.mFramesUntilNextRayCheck == 0) {
		constexpr f32 STEER_MULT = 1.5f;
		f32v2 steerVector = targetVelocity * STEER_MULT; //Look ahead
		IntersectionHit hit = world.tryGetRaycastIntersect(physCmp.getXYPosition(), physCmp.getXYPosition() + steerVector, physCmp.getZPosition());
		if (hit.didHit()) {
			// Something in the way!
			f32 angle = atan2(-hit.normal.y, -hit.normal.x) - atan2(steerVector.y, steerVector.x);
			// Large negative is positive
			if (angle < -M_PI) {
				angle = M_2_PI - angle;
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

	const float ARC_LENGTH = DEG_TO_RAD(175.0f);

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

void NavigationComponentSystem::update(entt::registry& registry, World& world) {
	// Update components
    auto view = registry.view<NavigationComponent, PhysicsComponent>();

    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        if (navCmp.mCurrentPoint < navCmp.mPath->numPoints) {
            auto& physCmp = view.get<PhysicsComponent>(entity);
			updateComponent(entity, navCmp, physCmp, world);
		}
	}
}

void NavigationComponent::setPathWithCallback(std::unique_ptr<Path> path, std::function<void(bool)> finishedCallback) {
	mPath = std::move(path);
	mCurrentPoint = 0;
	mFinishedCallback = finishedCallback;
}

void NavigationComponent::abortPath()
{
	mPath = nullptr;
	if (mFinishedCallback) {
		mFinishedCallback(false /*success*/);
		mFinishedCallback = nullptr;
	}
}
