#include "stdafx.h"
#include "DynamicCharacterController.h"

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletDynamics/Dynamics/btDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btIDebugDraw.h>

#include <cassert>

namespace {
	class FindGround : public btCollisionWorld::ContactResultCallback {
	public:
		FindGround(const DynamicCharacterController* controller,
			const btCollisionWorld* world);

		btScalar addSingleResult(btManifoldPoint& cp,
			const btCollisionObjectWrapper* colObj0, int partId0, int index0,
			const btCollisionObjectWrapper* colObj1, int partId1, int index1);
		btVector3 getInvNormal();

		bool mHaveGround = false;
		btVector3 mGroundPoint;
		btVector3 mStepPoint;
		btVector3 mStepNormal;

	private:
		void checkGround(const btManifoldPoint& cp);

		const DynamicCharacterController* mController;
		const btCollisionWorld* mWorld;
	};
}

DynamicCharacterController::DynamicCharacterController(btRigidBody* body,
	const btCapsuleShape* shape)
{
	mRigidBody = body;

	mShapeRadius = shape->getRadius();
	mShapeHalfHeight = shape->getHalfHeight();

	setupBody();
	resetStatus();
}

DynamicCharacterController::DynamicCharacterController()
{
	mRigidBody = nullptr;
	resetStatus();
}

void DynamicCharacterController::setupBody()
{
	assert(mRigidBody);
	mRigidBody->setSleepingThresholds(0.0, 0.0);
	mRigidBody->setAngularFactor(0.0);
	mGravity = mRigidBody->getGravity();
}

void DynamicCharacterController::updateAction(btCollisionWorld* collisionWorld,
	btScalar deltaTimeStep)
{
	FindGround groundSteps(this, collisionWorld);
	collisionWorld->contactTest(mRigidBody, groundSteps);
	mOnGround = groundSteps.mHaveGround;
	mGroundPoint = groundSteps.mGroundPoint;

	updateVelocity(deltaTimeStep);

	if (mOnGround) {
		/* Avoid going down on ramps, if already on ground, and clearGravity()
		is not enough */
		mRigidBody->setGravity({ 0, 0, 0 });
	}
	else {
		mRigidBody->setGravity(mGravity);
	}
}

void DynamicCharacterController::updateVelocity(float dt)
{
	btTransform transform;
	mRigidBody->getMotionState()->getWorldTransform(transform);
	btMatrix3x3& basis = transform.getBasis();
	/* Orthonormal basis - can just transpose to invert.
	Also Bullet does this in the btTransform::inverse */
	btMatrix3x3 inv = basis.transpose();

	btVector3 linearVelocity = inv * mRigidBody->getLinearVelocity();
   
    btVector3 desiredVelocity; // TODO: No sqrt
	if (!mOnGround && linearVelocity[2] < 0) {
		// TODO: Air resistance?
		desiredVelocity = linearVelocity;
	}
    else {
        // If we are on the ground, or we are moving up during a jump, allow control
		desiredVelocity = mMoveDirection * mMaxLinearVelocity;
	}
		
	// Use lerp instead of force so that we dont orbit
	// https://www.construct.net/en/blogs/ashleys-blog-2/using-lerp-delta-time-924
	linearVelocity = linearVelocity.lerp(desiredVelocity, 1.0f - pow(mAcceleration, dt));

	// Overspeed correction (Old method)
    /*btScalar speed2 = SQ(linearVelocity.x()) + SQ(linearVelocity.y());
    if (speed2 > mMaxLinearVelocity2) {
        btScalar correction = sqrt(mMaxLinearVelocity2 / speed2);
        linearVelocity[0] *= correction;
        linearVelocity[1] *= correction;
    }*/

	// TODO : Apply extra downward force when moving down a ramp?
	// No the force will make us slide like on ice, we need to possibly manually set the position?
	// Or perhaps we can increase friction based on the normal of the slope?

	if (mJump) {
		linearVelocity += mJumpSpeed * mJumpDir;
		mJump = false;
	}

	mRigidBody->setLinearVelocity(basis * linearVelocity);
}

void DynamicCharacterController::debugDraw(btIDebugDraw* debugDrawer)
{
	
}

void DynamicCharacterController::setMovementDirection(
	const btVector3& walkDirection)
{
	mMoveDirection = walkDirection;
	mMoveDirection.setZ(0);
	if (!mMoveDirection.fuzzyZero()) {
		mMoveDirection.normalize();
	}
}

void DynamicCharacterController::setMaxLinearVelocity(f32 maxVelocity) {
	mMaxLinearVelocity = maxVelocity;
}

const btVector3& DynamicCharacterController::getMovementDirection() const
{
	return mMoveDirection;
}

void DynamicCharacterController::resetStatus()
{
	mMoveDirection.setValue(0, 0, 0);
	mJump = false;
	mOnGround = false;
}

bool DynamicCharacterController::canJump() const
{
	return mOnGround;
}

void DynamicCharacterController::jump(const btVector3& dir)
{
	if (!canJump()) {
		return;
	}

	mJump = true;

	mJumpDir = dir;
	if (dir.fuzzyZero()) {
		mJumpDir.setValue(0, 0, 1);
	}
	mJumpDir.normalize();
}

const btRigidBody* DynamicCharacterController::getBody() const
{
	return mRigidBody;
}


namespace {
	FindGround::FindGround(
		const DynamicCharacterController* controller, const btCollisionWorld* world)
	{
		mController = controller;
		mWorld = world;
	}

	btScalar FindGround::addSingleResult(btManifoldPoint& cp,
		const btCollisionObjectWrapper* colObj0, int partId0, int index0,
		const btCollisionObjectWrapper* colObj1, int partId1, int index1)
	{
		assert(colObj0->m_collisionObject == mController->getBody() && "Character controller bad single result");
		if (colObj0->m_collisionObject == mController->getBody()) {
			/* The first object should always be the rigid body of the
			controller, but check anyway.
			In case the body is the second object, we cannot use the collision
			information, because Bullet provides only the normal of the second
			point. */
			checkGround(cp);
		}

		// By looking at btCollisionWorld.cpp, it seems Bullet ignores this value
		return 0;
	}

	void FindGround::checkGround(const btManifoldPoint& cp)
	{
		if (mHaveGround) {
			return;
		}

		btTransform inverse = mController->getBody()->getWorldTransform().inverse();
		btVector3 localPoint = inverse(cp.m_positionWorldOnB);
		localPoint[2] += mController->mShapeHalfHeight;

		float r = localPoint.length();
		float cosTheta = localPoint[2] / r;

		if (fabs(r - mController->mShapeRadius) <= mController->mRadiusThreshold
			&& cosTheta < mController->mMaxCosGround)
		{
			mHaveGround = true;
			mGroundPoint = cp.m_positionWorldOnB;
		}
	}

	btVector3 FindGround::getInvNormal()
	{
		return btVector3(0, 0, 0);
	}

}
