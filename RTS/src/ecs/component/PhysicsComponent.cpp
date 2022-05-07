#include "stdafx.h"
#include "PhysicsComponent.h"

#include "World.h"
#include "ecs/EntityComponentSystem.h"

#include "world/TileRepository.h"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>

constexpr float MIN_Z_SPEED = -0.24f;
constexpr float TOP_COLLISION_THRESHOLD = 0.75f;
constexpr float TOP_COLLISION_DEPTH = 1.0f - TOP_COLLISION_THRESHOLD;
constexpr float REFILTER_HEIGHT_CHANGE = 0.2f;
// This prevents tunelling when falling
static_assert(1.0f + MIN_Z_SPEED > TOP_COLLISION_THRESHOLD);

KEG_ENUM_DEF(ColliderShapes, ColliderShapes, kt) {
    kt.addValue("none", ColliderShapes::NONE);
    kt.addValue("circle", ColliderShapes::CIRCLE);
}
static_assert(e_cast(ColliderShapes::COUNT) == 2, "Update def");

KEG_TYPE_DEF_SAME_NAME(PhysicsComponentDef, kt) {
    kt.addValue("collider_shape", keg::Value::custom(offsetof(PhysicsComponentDef, colliderShape), "ColliderShapes", true));
	kt.addValue("collider_radius", keg::Value::basic(offsetof(PhysicsComponentDef, colliderRadius), keg::BasicType::F32));
    kt.addValue("is_static", keg::Value::basic(offsetof(PhysicsComponentDef, isStatic), keg::BasicType::BOOL));
}

constexpr float VEL_DAMPING = 0.75f;
constexpr float GRAVITY_FORCE = 0.03f;

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

void resolveCircleTileCollision(const f32v2& tileCenter, const Tile* tile, PhysicsComponent& cmp, OUT bool& isOnTile) {
    const TileCollider* collider = tile->tryGetColliderMainThread();
    TileCollisionShape shape = TileCollisionShape::BOX;
    float tileCollisionRadius = 0.5f; // Defualt for box
    if (collider) {
        shape = collider->shape;
        tileCollisionRadius = collider->dims.x; // TODO: better?
    }
    else {
        TileID groundId = tile->getLayersMainThread()[TILE_LAYER_GROUND];
        if (groundId == TILE_ID_NONE) {
            return;
        }
    }

    float colliderRadius = cmp.mCollisionRadius;
    const f32v2& colliderCenter = cmp.getXYPosition();
    f32v2 offsetToCollider = colliderCenter - tileCenter;
    const f32 baseZPosition = tile->getBaseZPositionUncompressedMainThread();

	bool isCollidingWithTop = false;
	float zOffset = cmp.getZPosition() - baseZPosition;
	if (zOffset > 0.0f) {
		// We are above, do nothing
		return;
	}
	else if (zOffset > -TOP_COLLISION_DEPTH || (zOffset < -10.0f && cmp.getZPosition() < -10.0f)) { // If we are colliding with top, or stuck underneath world (fix tunnel)
		// We are colliding with the top, snap us up
		// TODO: we could compare this to the depression of the XY so we don't pop straight up on the corners when climbing?
		isCollidingWithTop = true;
		// When colliding with top, our feet are colliding and are smaller
		// TODO: Make this dynamic?
		colliderRadius *= 0.3f;
	}

	switch (shape)
    {
		// Circle falls through to check ground
		case TileCollisionShape::CIRCLE: {
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
        }
        [[fallthrough]];
        case TileCollisionShape::NONE:
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
                    cmp.setZPosition(baseZPosition);
                    cmp.setZVelocity(0.0f);
                    isOnTile = true;
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
        default:
			assert(false); // Unhandled shape
            break;
	}
    static_assert((int)TileCollisionShape::COUNT == 3, "Update");
}

// TODO: Measure perf of this vs non inline vs macro
inline void updateComponent(World& world, PhysicsComponent& cmp) {
    const f32v2& xyVel = cmp.getLinearVelocity();

    // TODO: TestBit
    if (/*(cmp.mFlags & e_cast(PhysicsComponentFlag::LOCK_DIR_TO_VELOCITY)) && */(glm::abs(xyVel.x) > 0.0001f || glm::abs(xyVel.y) >= 0.0001f)) {
        cmp.mDir = glm::normalize(lerp(cmp.mDir, glm::normalize(xyVel), 0.7f));
    }

    // Handle gravity and Z velocity
    if (cmp.mZVelocity < MIN_Z_SPEED) {
        cmp.mZVelocity = MIN_Z_SPEED;
    }
    cmp.mZPosition += cmp.mZVelocity;
    cmp.mZVelocity -= GRAVITY_FORCE;


    const f32v2& xyPosition = cmp.getXYPosition();

    // TODO: Handle larger colliders
    const f32v2 cornerPositions[4] = {
        xyPosition + f32v2(-0.5f,-0.5f), // Bottom left
        xyPosition + f32v2(0.5f,-0.5f), // Bottom right
        xyPosition + f32v2(-0.5f, 0.5f), // Top left
        xyPosition + f32v2(0.5f, 0.5f), // Top right
    };

    // TODO: This method has issues if large group of units is trying to walk into a wall, probably need impulses instead
    bool isOnTile = false;
    for (int i = 0; i < 4; ++i) {
        const Tile* tile = world.tryGetTileAtWorldPos(cornerPositions[i]);
        if (tile) {
            // TODO: This can reduntantly collide
            const f32v2 tileCenter(floor(cornerPositions[i].x) + 0.5f, floor(cornerPositions[i].y) + 0.5f);
            resolveCircleTileCollision(tileCenter, tile, cmp, isOnTile);
        }
    }

    // Resolve terrain collision
    const WorldGrid& grid = world.getWorldGrid();
    f32 terrainHeight;
    constexpr f32 SNAP_THRESHOLD = 0.01f;
    if (grid.tryComputeHeightAtPoint(xyPosition, &terrainHeight)) {
        if (terrainHeight >= cmp.mZPosition - SNAP_THRESHOLD) {
            cmp.mZPosition = terrainHeight;
            cmp.setZVelocity(0.0f);
            cmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
        }
        else {
            if (isOnTile) {
                cmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
            }
            else {
                cmp.mFlags.clearBit(PhysicsComponentFlag::IS_ON_GROUND);
            }
        }
    }
    else {
        cmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
        cmp.setZVelocity(0.0f);
    }

    // Refilters for pseudo3d collision
    if (fabs(cmp.mZPosition - cmp.mLastZPositionAtRefilter) > REFILTER_HEIGHT_CHANGE) {
        cmp.mLastZPositionAtRefilter = cmp.mZPosition;
        b2Fixture* f = cmp.mBody->GetFixtureList();
        f->Refilter();
    }

}


PhysicsSystem::PhysicsSystem(World& world)
	: mWorld(world) {

}

void PhysicsSystem::updateFrameBegin(entt::registry& registry) {
    // Store prev position so we can frameAlpha
    registry.view<PhysicsComponent>().each([&](auto& cmp) {
        cmp.mPrevXYPosition = cmp.getXYPosition();
        cmp.mPrevZPosition = cmp.mZPosition;
    });
}

void PhysicsSystem::update(entt::registry& registry) {
	// Update components
	registry.view<PhysicsComponent>().each([&](auto& cmp) {
        updateComponent(mWorld, cmp);
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
        mBody->SetLinearDamping(0.3f);
    }
	mPrevXYPosition = centerPosition;
	mPrevZPosition = mZPosition;
}

void PhysicsComponent::addCollider(entt::entity entityId, ColliderShapes shape, const float halfWidth) {

	// Init physics body
	switch (shape) {
		case ColliderShapes::CIRCLE: {
			b2CircleShape dynamicCircle;
			dynamicCircle.m_radius = halfWidth;
			mCollisionRadius = dynamicCircle.m_radius;

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &dynamicCircle;
			fixtureDef.density = 1.0f;
			fixtureDef.userData.pointer = static_cast<uintptr_t>(entityId);

			mBody->CreateFixture(&fixtureDef);
			break;
		}
		case ColliderShapes::NONE:
			ASSERT_FAIL; // Invalid collider type
		default:
			ASSERT_FAIL; // Need to add collider type
	}

}
