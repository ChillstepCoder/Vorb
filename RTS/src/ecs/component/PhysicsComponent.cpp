#include "stdafx.h"
#include "PhysicsComponent.h"

#include "World.h"
#include "EntityComponentSystem.h"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>

// TODO: Capsule collision
inline void handleCollision2D(PhysicsComponent& cmp1, PhysicsComponent& cmp2) {
	//// We add radius since position is the top left corner
	//const glm::vec2 distVec = cmp2.getPosition() - cmp1.getPosition();
	//const float dist = glm::length(distVec);
	//const float totalRadius = cmp1.mCollisionRadius + cmp2.mCollisionRadius;
	//const float collisionDepth = totalRadius - dist;
	//// Check for collision
	//if (collisionDepth > 0) {
	//	const glm::vec2 distDir = distVec / dist;

	//	// Push away the balls based on ratio of mMasses
	//	// TODO: Could/should we encode this in the impulse?
	//	// TODO: 3D?
	//	const f32v2 offset = distDir * collisionDepth * 0.5f;
	//	cmp1.getPosition() -= offset * (cmp2.mMass / cmp1.mMass);
	//	cmp2.mPosition += offset * (cmp1.mMass / cmp2.mMass);

	//	// Calculate deflection. http://stackoverflow.com/a/345863
	//	// Fixed thanks to youtube user Sketchy502
	//	const float aci = glm::dot(cmp1.mVelocity, distDir);
	//	const float bci = glm::dot(cmp2.mVelocity, distDir);

	//	const float acf = (aci * (cmp1.mMass - cmp2.mMass) + 2 * cmp2.mMass * bci) / (cmp1.mMass + cmp2.mMass);
	//	const float bcf = (bci * (cmp2.mMass - cmp1.mMass) + 2 * cmp1.mMass * aci) / (cmp1.mMass + cmp2.mMass);

	//	cmp1.mVelocity += (acf - aci) * distDir;
	//	cmp2.mVelocity += (bcf - bci) * distDir;
	//}
}

// TODO: Measure perf of this vs non inline vs macro
inline void updateComponent(World& world, PhysicsComponent& cmp, float deltaTime) {
	const f32v2& vel = cmp.getLinearVelocity();
	// TODO: TestBit
	if ((cmp.mFlags & enum_cast(PhysicsComponentFlag::LOCK_DIR_TO_VELOCITY)) && (glm::abs(vel.x) > 0.0001f || glm::abs(vel.y) >= 0.0001f)) {
		cmp.mDir = glm::normalize(vel);
	}

	//if (cmp.mPosition.z <= 0.0f) {
	//}
	//cmp.mPosition += cmp.mVelocity;
	//if (cmp.mFrictionEnabled) {
	//	cmp.mVelocity -= cmp.mVelocity * (1.0f - cmp.mFrictionCoef) * deltaTime; // TODO: Deterministic?
	//}

	const f32v2& position = cmp.getPosition();

	// TODO: Handle larger colliders
	const f32v2 cornerPositions[4] = {
		position + f32v2(-0.5f,-0.5f), // Bottom left
		position + f32v2( 0.5f,-0.5f), // Bottom right
		position + f32v2(-0.5f, 0.5f), // Top left
		position + f32v2( 0.5f, 0.5f), // Top right
	};

	const float VEL_DAMPING = 0.75f;

	// TODO: This method has issues if large group of units is trying to walk into a wall, probably need impulses instead
	const float circleRadius = cmp.mCollisionRadius;
	for (int i = 0; i < 4; ++i) {
		TileHandle handle = world.getTileHandleAtWorldPos(cornerPositions[i]);
		// TODO: CollisionMap
		for (int l = 0; l < 3; ++l) {
			if (TileRepository::getTileData(handle.tile.layers[l]).collisionBits) {
				const f32v2 tileCenter(floor(cornerPositions[i].x) + 0.5f, floor(cornerPositions[i].y) + 0.5f);
				f32v2 offsetToCircle = position - tileCenter;
				offsetToCircle.x = vmath::clamp(offsetToCircle.x, -0.5f, 0.5f);
				offsetToCircle.y = vmath::clamp(offsetToCircle.y, -0.5f, 0.5f);
				const f32v2 closestPoint = tileCenter + offsetToCircle;
				const f32v2 offsetToWall = closestPoint - position;
				const float dx2 = offsetToWall.x * offsetToWall.x;
				const float dy2 = offsetToWall.y * offsetToWall.y;
				if (dx2 + dy2 < circleRadius * circleRadius) {
					// Collision!
					b2Vec2 impulse;
					b2Vec2 currentVelocity = cmp.mBody->GetLinearVelocity();
					if (dx2 > dy2) {
						// X collision
						if (offsetToWall.x < 0.0f) {
							// Colliding with left wall
							impulse.x = 0.1f;
							if (currentVelocity.x < 0.0f) {
								currentVelocity.x = -currentVelocity.x * VEL_DAMPING;
								cmp.mBody->SetLinearVelocity(currentVelocity);
							}
							const float collisionDepth = circleRadius + offsetToWall.x;
							cmp.mBody->SetTransform(b2Vec2(position.x + collisionDepth, position.y), 0.0f);
						}
						else {
							// Colliding with right wall
							impulse.x = -0.1f;
							if (currentVelocity.x > 0.0f) {
								currentVelocity.x = -currentVelocity.x * VEL_DAMPING;
								cmp.mBody->SetLinearVelocity(currentVelocity);
							}
							const float collisionDepth = circleRadius - offsetToWall.x;
							cmp.mBody->SetTransform(b2Vec2(position.x - collisionDepth, position.y), 0.0f);
						}
						impulse.y = 0.0f;
					}
					else {
						// Y collision
						if (offsetToWall.y < 0.0f) {
							// Colliding with bottom wall
							impulse.y = 0.1f;
							if (currentVelocity.y < 0.0f) {
								currentVelocity.y = -currentVelocity.y * VEL_DAMPING;
								cmp.mBody->SetLinearVelocity(currentVelocity);
							}
							const float collisionDepth = circleRadius + offsetToWall.y;
							cmp.mBody->SetTransform(b2Vec2(position.x, position.y + collisionDepth), 0.0f);
						}
						else {
							// Colliding with top wall
							impulse.y = -0.1f;
							if (currentVelocity.y > 0.0f) {
								currentVelocity.y = -currentVelocity.y * VEL_DAMPING;
								cmp.mBody->SetLinearVelocity(currentVelocity);
							}
							const float collisionDepth = circleRadius - offsetToWall.y;
							cmp.mBody->SetTransform(b2Vec2(position.x, position.y - collisionDepth), 0.0f);
						}
						impulse.x = 0.0f;
					}
				}
			}
		}
	}
}


PhysicsSystem::PhysicsSystem(World& world)
	: mWorld(world) {

}

void PhysicsSystem::update(entt::registry& registry, float deltaTime) {
	// THIS IS NOW HANDLED BY BOX2D

	// Collision
	// TODO: Spatial Partition
	// Skip default element
	/*std::vector<ComponentPairing>::iterator it = _components.begin() + 1;
	while (it != _components.end()) {
		if (isValid(*it)) {
			auto compareIt = it;
			while (++compareIt != _components.end()) {
				if (isValid(*compareIt)) {
					handleCollision2D(it->second, compareIt->second);
				}
			}
		}
		++it;
	}*/

	// Update components
	registry.view<PhysicsComponent>().each([&](auto& cmp) {
        updateComponent(mWorld, cmp, deltaTime);
	});
}

PhysicsComponent::PhysicsComponent(World& world, const f32v2& centerPosition, bool isStatic) {
    b2BodyDef bodyDef;
    if (isStatic) {
        bodyDef.type = b2_staticBody;
        bodyDef.position.Set(centerPosition.x, centerPosition.y);
        mBody = world.createPhysBody(&bodyDef);
    }
    else {
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(centerPosition.x, centerPosition.y);
        mBody = world.createPhysBody(&bodyDef);
        mBody->SetLinearDamping(0.1f);
    }
}

void PhysicsComponent::addCollider(entt::entity entityId, ColliderShapes shape, const float halfWidth) {

	// Init physics body
	switch (shape) {
		case ColliderShapes::SPHERE: {
			b2CircleShape dynamicCircle;
			dynamicCircle.m_radius = halfWidth;
			mCollisionRadius = dynamicCircle.m_radius;

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &dynamicCircle;
			fixtureDef.density = 1.0f;
			fixtureDef.userData = reinterpret_cast<void*>(entityId);

			mBody->CreateFixture(&fixtureDef);
			break;
		}
		case ColliderShapes::NONE:
			ASSERT_FAIL; // Invalid collider type
		default:
			ASSERT_FAIL; // Need to add collider type
	}

}
