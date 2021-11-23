#pragma once

#include <glm/gtx/rotate_vector.hpp>

// TODO: Eigen for spline fitting 
// https://eigen.tuxfamily.org/dox/unsupported/group__Splines__Module.html

template <class T>
class Tweener {
public:
    Tweener(T start, f32 maxSpeed = 0.3f, f32 accel = 0.1f) : mCurr(start), mTarget(start), mMaxSpeed(maxSpeed), mAccel(accel) { }

    void setAcceleration(f32 accel) { mAccel = accel; }
    void setMaxSpeed(f32 maxSpeed) { mMaxSpeed = maxSpeed; }
    void setTarget(T target) { mTarget = target; }
    virtual void update(f32 deltaTime) {
        const T offsetToTarget = mTarget - mCurr;
        const float distanceToTarget = glm::length(offsetToTarget);
        if (distanceToTarget < 0.0005f) {
            mCurrentVelocity = T(0.0f);
            return;
        }
        const T normalToTarget = offsetToTarget / distanceToTarget;

        T maxTargetVelocity = normalToTarget * mMaxSpeed;

        // How fast are we going in the correct direction?
        const T projectedVelocity = (glm::dot(mCurrentVelocity, maxTargetVelocity) / glm::length2(maxTargetVelocity)) * maxTargetVelocity;
        const f32 projectedSpeed = glm::length(projectedVelocity);

        bool isDecelerating = false;
        if (distanceToTarget < mMaxSpeed * 10.0f) {
            maxTargetVelocity *= (distanceToTarget / (mMaxSpeed * 10.0f));
            if (projectedSpeed >= glm::length(maxTargetVelocity)) {
                isDecelerating = true;
            }
        }

        // Smooth accelerate, abrupt decelerate
        if (isDecelerating) {
            mCurrentVelocity = vmath::lerp(mCurrentVelocity, maxTargetVelocity, 0.9f);
        }
        else {
            mCurrentVelocity = vmath::lerp(mCurrentVelocity, maxTargetVelocity, 0.1f);
        }
        mCurr = mCurr + mCurrentVelocity;
    }

    const T& getCurr() const { return mCurr; }
    const T& getTarget() const { return mTarget; }

    T mCurr;
    T mTarget;
    T mCurrentVelocity = T(0.0f);
    f32 mAccel;
    f32 mMaxSpeed;
};

template <class T>
class SphericalTweener : public Tweener<T> {
public:
    SphericalTweener(T start, f32 maxSpeed = 0.3f, f32 accel = 0.1f) : Tweener<T>(start, maxSpeed, accel) {  }
    void update(f32 deltaTime) override {
        Tweener<T>::mCurr = vmath::lerp(Tweener<T>::mCurr, Tweener<T>::mTarget, Tweener<T>::mAccel);
        Tweener<T>::mCurr = glm::normalize(Tweener<T>::mCurr);
    }
};
