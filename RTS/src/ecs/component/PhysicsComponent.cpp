#include "stdafx.h"
#include "PhysicsComponent.h"

#include "World.h"
#include "EntityComponentSystem.h"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>

constexpr float MIN_Z_SPEED = -0.24f;
constexpr float TOP_COLLISION_THRESHOLD = 0.75f;
constexpr float TOP_COLLISION_DEPTH = 1.0f - TOP_COLLISION_THRESHOLD;
// This prevents tunelling when falling
static_assert(1.0f + MIN_Z_SPEED > TOP_COLLISION_THRESHOLD);

// Collision info

float TileCollisionShapeRadii[(int)TileCollisionShape::COUNT] = {
	0.0f,   // NONE
	0.5f,   // BOX
	0.1f,   // SMALL_CIRCLE
	0.175f, // MEDIUM_CIRCLE
};
static_assert((int)TileCollisionShape::COUNT == 4, "Update");

constexpr float VEL_DAMPING = 0.75f;
constexpr float GRAVITY_FORCE = 0.01f;

// This is only kept as a reference for 2D collision detection
// TODO: eventually remove this / replace with a common utility
//inline void handleCollision2D(PhysicsComponent& cmp1, PhysicsComponent& cmp2) {
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
//}

void resolveCircleTileCollision(const f32v2& tileCenter, f32 tileZTop, TileCollisionShape tileShape, PhysicsComponent& cmp) {
    float colliderRadius = cmp.mCollisionRadius;
    const f32v2& colliderCenter = cmp.getXYPosition();
    f32v2 offsetToCollider = colliderCenter - tileCenter;
	const float tileCollisionRadius = TileCollisionShapeRadii[(int)tileShape];

	bool isCollidingWithTop = false;
	float zOffset = cmp.getZPosition() - tileZTop;
	if (zOffset > 0.0f) {
		// We are above, do nothing
		return;
	}
	else if (zOffset > -TOP_COLLISION_DEPTH) {
		// We are colliding with the top, snap us up
		// TODO: we could compare this to the depression of the XY so we don't pop straight up on the corners when climbing?
		isCollidingWithTop = true;
		// When colliding with top, our feet are colliding and are smaller
		// TODO: Make this dynamic?
		colliderRadius *= 0.3f;
	}

	switch (tileShape)
	{
		case TileCollisionShape::FLOOR:
		case TileCollisionShape::BOX: {
            offsetToCollider.x = vmath::clamp(offsetToCollider.x, -0.5f, 0.5f);
            offsetToCollider.y = vmath::clamp(offsetToCollider.y, -0.5f, 0.5f);

            const f32v2 closestPoint = tileCenter + offsetToCollider;
            const f32v2 offsetToWall = closestPoint - colliderCenter;
            const float dx2 = offsetToWall.x * offsetToWall.x;
            const float dy2 = offsetToWall.y * offsetToWall.y;

			// Shrink collider radius if is colliding with top

            if (dx2 + dy2 < SQ(colliderRadius)) {
                // Just pop up
                // TODO: Move up smoother, always counter gravity
                if (isCollidingWithTop) {
                    cmp.setZPosition(tileZTop);
					cmp.setZVelocity(0.0f);
                    return;
                }
                // Collision!
                b2Vec2 currentVelocity = cmp.mBody->GetLinearVelocity();
                if (dx2 > dy2) {
                    // X collision
                    if (offsetToWall.x < 0.0f) {
                        // Colliding with left wall
                        if (currentVelocity.x < 0.0f) {
                            currentVelocity.x = -currentVelocity.x * VEL_DAMPING;
                            cmp.mBody->SetLinearVelocity(currentVelocity);
                        }
                        const float collisionDepth = colliderRadius + offsetToWall.x;
						cmp.setXYPosition(f32v2(colliderCenter.x + collisionDepth, colliderCenter.y));
                    }
                    else {
                        // Colliding with right wall
                        if (currentVelocity.x > 0.0f) {
                            currentVelocity.x = -currentVelocity.x * VEL_DAMPING;
                            cmp.mBody->SetLinearVelocity(currentVelocity);
                        }
                        const float collisionDepth = colliderRadius - offsetToWall.x;
						cmp.setXYPosition(f32v2(colliderCenter.x - collisionDepth, colliderCenter.y));
                    }
                }
                else {

                    // Y collision
                    if (offsetToWall.y < 0.0f) {
                        // Colliding with bottom wall
                        if (currentVelocity.y < 0.0f) {
                            currentVelocity.y = -currentVelocity.y * VEL_DAMPING;
                            cmp.mBody->SetLinearVelocity(currentVelocity);
                        }
                        const float collisionDepth = colliderRadius + offsetToWall.y;
						cmp.setXYPosition(f32v2(colliderCenter.x, colliderCenter.y + collisionDepth));
                    }
                    else {
                        // Colliding with top wall
                        if (currentVelocity.y > 0.0f) {
                            currentVelocity.y = -currentVelocity.y * VEL_DAMPING;
                            cmp.mBody->SetLinearVelocity(currentVelocity);
                        }
                        const float collisionDepth = colliderRadius - offsetToWall.y;
						cmp.setXYPosition(f32v2(colliderCenter.x, colliderCenter.y - collisionDepth));
                    }
                }
            }
			break;
		}
		// Circle falls through
		case TileCollisionShape::SMALL_CIRCLE:
		case TileCollisionShape::MEDIUM_CIRCLE: {
			const float offset2 = glm::dot(offsetToCollider, offsetToCollider);
			const float totalRadius = colliderRadius + tileCollisionRadius;
			if (offset2 < SQ(totalRadius)) {
				const float offset = sqrt(offset2);
                f32v2 impulseNormal = offsetToCollider / offset;
				f32v2 currentVelocity = cmp.getLinearVelocity();
				float collisionDepth = totalRadius - offset;
                // Push away
				cmp.setXYPosition(colliderCenter + impulseNormal * collisionDepth);

                // Calcuate deflection
                float vDotN = glm::dot(currentVelocity, impulseNormal);
                cmp.setLinearVelocity(currentVelocity - vDotN * 2.0f * impulseNormal);
			}
			break;
		}

        default:
			assert(false); // Unhandled shape
            break;
	}
    static_assert((int)TileCollisionShape::COUNT == 4, "Update");
}

// TODO: Measure perf of this vs non inline vs macro
inline void updateComponent(World& world, PhysicsComponent& cmp, float deltaTime) {
	const f32v2& xyVel = cmp.getLinearVelocity();
	// TODO: TestBit
	if ((cmp.mFlags & enum_cast(PhysicsComponentFlag::LOCK_DIR_TO_VELOCITY)) && (glm::abs(xyVel.x) > 0.0001f || glm::abs(xyVel.y) >= 0.0001f)) {
		cmp.mDir = glm::normalize(xyVel);
	}

    // Handle gravity and Z velocity
    if (cmp.mZVelocity < MIN_Z_SPEED) {
        cmp.mZVelocity = MIN_Z_SPEED;
    }
	cmp.mZPosition += cmp.mZVelocity * deltaTime;
	cmp.mZVelocity -= GRAVITY_FORCE * deltaTime;


	const f32v2& xyPosition = cmp.getXYPosition();

	// TODO: Handle larger colliders
	const f32v2 cornerPositions[4] = {
		xyPosition + f32v2(-0.5f,-0.5f), // Bottom left
		xyPosition + f32v2( 0.5f,-0.5f), // Bottom right
		xyPosition + f32v2(-0.5f, 0.5f), // Top left
		xyPosition + f32v2( 0.5f, 0.5f), // Top right
	};

	// TODO: This method has issues if large group of units is trying to walk into a wall, probably need impulses instead
	for (int i = 0; i < 4; ++i) {
		TileHandle handle = world.getTileHandleAtWorldPos(cornerPositions[i]);
		if (handle.isValid()) {
			const f32v2 tileCenter(floor(cornerPositions[i].x) + 0.5f, floor(cornerPositions[i].y) + 0.5f);
			// TODO: CollisionMap
			for (int l = 0; l < 3; ++l) {
				TileID tileId = handle.tile.layers[l];
				if (tileId != TILE_ID_NONE) {
					const TileData tileData = TileRepository::getTileData(tileId);
					float topZPos = handle.tile.baseZPosition;
					if (tileData.collisionShape != TileCollisionShape::FLOOR) {
						topZPos += tileData.colliderHeight;
					}
					resolveCircleTileCollision(tileCenter, topZPos, tileData.collisionShape, cmp);
				}
			}
		}
		else {
			// Invalid handle, freeze physics
			cmp.mZVelocity = 0.0f;
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
