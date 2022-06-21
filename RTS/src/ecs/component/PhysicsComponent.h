#pragma once
#include "actor/ActorTypes.h"

class World;
class b2Body;
class EntityComponentSystem;

enum class PhysicsComponentFlag : ui8 {
	IS_ON_GROUND         = 1 << 0,
	LOCK_DIR_TO_VELOCITY = 1 << 1,
};

enum class ColliderShapes {
	NONE,
	CIRCLE,
	COUNT
};
KEG_ENUM_DECL(ColliderShapes);

// TODO: Investigate the cost of stable pointer ( using in_place_delete = std::true_type;)  https://skypjack.github.io/entt/md_docs_md_entity.html
class PhysicsComponent {
public:
	PhysicsComponent(World& world, const f32v2& centerPosition, bool isStatic);

	void addCollider (entt::entity entityId, ColliderShapes shape, const float halfWidth);

    void setXYPosition(const f32v2& pos) {
		mPosition.x = pos.x;
		mPosition.y = pos.y;
    }
	void setLinearVelocity(const f32v2& vel) {
		mLinearVelocity = vel;
	}
	void setZPosition(f32 z) {
		mZPosition = z;
	}
	void setZVelocity(f32 vel) {
		mZVelocity = vel;
	}

	const f32v2 getXYPosition() const {
		return f32v2(mPosition.x, mPosition.y);
	}
	const float getZPosition() const {
		return mZPosition;
	}
	const f32v3 getPosition() const {
		const f32v2& xy = getXYPosition();
		return f32v3(xy.x, xy.y, mZPosition);
	}

	f32v2 getXYInterpolated(f32 frameAlpha) const {
        const f32v2& nextXY = getXYPosition();
        f32v2 interpolatedXY;
		// Comparisons are to fix floating point math lerp errors
        interpolatedXY.x = (nextXY.x == mPrevXYPosition.x) ? nextXY.x : vmath::lerp(mPrevXYPosition.x, nextXY.x, frameAlpha);
        interpolatedXY.y = (nextXY.y == mPrevXYPosition.y) ? nextXY.y : vmath::lerp(mPrevXYPosition.y, nextXY.y, frameAlpha);
		return interpolatedXY;
	}
	float getZInterpolated(f32 frameAlpha) const {
		return (mPrevZPosition == mZPosition) ? mZPosition : vmath::lerp(mPrevZPosition, mZPosition, frameAlpha);
	}

	f32v3 getPositionInterpolated(f32 frameAlpha) {
		f32v2 xy = getXYInterpolated(frameAlpha);
		return f32v3(xy.x, xy.y, getZInterpolated(frameAlpha));
	}

	const f32v2& getLinearVelocity() const {
		return mLinearVelocity;
	}

    void teleportToPoint(const f32v2& worldPos) {
		setXYPosition(worldPos);
		if (mZPosition < 0.0f) {
			mZPosition = 2.0f;
		}
    }

    void teleportToPoint(const f32v3& worldPos) {
		setXYPosition(f32v2(worldPos.x, worldPos.y));
		mZPosition = worldPos.z;
    }

	bool isOnGround() const { return mFlags.isBitSet(PhysicsComponentFlag::IS_ON_GROUND); }

	f32v3 mPosition = f32v3(0.0f);
	f32v2 mPrevXYPosition = f32v2(0.0f);
	f32v2 mLinearVelocity = f32v2(0.0f);
	f32 mPrevZPosition = 0.0f;
	f32v2 mDir = f32v2(0.0f, -1.0f);
    f32 mCollisionRadius = 0.0f;
    f32 mCollisionHeight = 1.0f;
    f32 mZPosition = 0.0f;
    f32 mZVelocity = 0.0f;
	f32 mLastZPositionAtRefilter = 0.0f;
    b2Body* mBody = nullptr;

    BitFlags<PhysicsComponentFlag> mFlags;
    ui8 mQueryActorTypes = ACTORTYPE_NONE;

};

struct PhysicsComponentDef {
    ColliderShapes colliderShape;
    float colliderRadius;
    bool isStatic;
};
KEG_TYPE_DECL(PhysicsComponentDef);

class PhysicsSystem {
public:
	PhysicsSystem(World& world);

    void updateFrameBegin(entt::registry& registry);
	void update(entt::registry& registry);

	World& mWorld;
};
