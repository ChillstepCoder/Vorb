#include "stdafx.h"
#include "ParticleSystem.h"

#include "Random.h"

const float GRAVITY_CONSTANT = 0.01f;

ParticleSystem::ParticleSystem(const f32v3& rootPosition, const f32v3& initialVelocity, const ParticleSystemData& systemData) :
    mRootPosition(rootPosition),
    mSystemData(systemData),
    mInitialVelocity(initialVelocity)
{
    mParticles.reserve(systemData.maxParticles);
    mEmitRate = mSystemData.duration / mSystemData.maxParticles;

    // One shot should emit on init
    if (mSystemData.duration == 0.0f) {
        tryEmitParticles();
    }
}

ParticleSystem::~ParticleSystem()
{
    // TODO: Would be nice to have RAII wrapper around buffer objects
    if (mVbo) {
        glDeleteBuffers(1, &mVbo);
    }
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
    }
}

bool ParticleSystem::update(float deltaTime)
{
    mCurrentTime += sElapsedSecondsSinceLastFrame;
    // Handle looping logic
    if (mCurrentTime > mSystemData.duration) {
        if (mSystemData.isLooping) {
            mCurrentTime -= mSystemData.duration;
        }
        else {
            mIsStopped = true;
        }
    }

    if (!mIsStopped) {
        tryEmitParticles();
    }
    
    // Motion
    const float gravityForce = GRAVITY_CONSTANT * deltaTime * mSystemData.gravityMult;
    const float restingForce = gravityForce * 4.0f;
    for (unsigned i = 0; i < mParticles.size();) {
        Particle& particle = mParticles[i];
        // TODO: Real time lifetime
        particle.mLifeTime += sElapsedSecondsSinceLastFrame;
        if (particle.mLifeTime >= mSystemData.particleLifetimeSecondsRange.y) {
            particle = mParticles.back();
            mParticles.pop_back();
            continue;
        }
        // x, y, z = 16 bit velocity  w
        particle.mVelocity.z -= gravityForce;
        particle.mVelocity *= mSystemData.airDamping;
        particle.mPosition += particle.mVelocity * deltaTime;
        // Bounce off the ground
        if (particle.mPosition.z <= 0.0f) {
            particle.mPosition.z = -particle.mPosition.z;
            particle.mVelocity.z = -particle.mVelocity.z;
            particle.mVelocity *= mSystemData.hitDamping;
            if (particle.mVelocity.z <= restingForce) {
                particle.mVelocity.z = 0.0f;
            }
            // TODO: Generate collision event
        }
        ++i;
    }

    // TODO: Custom logic
    switch (mSystemData.type) {
        case ParticleSystemType::SIMPLE:
            break;
        case ParticleSystemType::FIRE:
            break;
    }

    mDirty = true;

    if (mIsStopped) {
        return mParticles.size() == 0;
    }
    return false;
}

void ParticleSystem::stop() {
    mIsStopped = true;
}

void ParticleSystem::tryEmitParticles()
{
    mEmitTimer -= sElapsedSecondsSinceLastFrame;
    while (mEmitTimer < 0.0f) {
        mEmitTimer += mEmitRate;
        if (mParticles.size() < mSystemData.maxParticles) {
            emitParticle();
        }
        else {
            return;
        }
    }
}

inline void ParticleSystem::emitParticle() {
    mParticles.emplace_back();
    Particle& particle = mParticles.back();
    // TODO: Random emit
    particle.mPosition = mRootPosition;

    const float angleDeg = ((Random::getCachedRandomf() * 2.0f) - 1.0f) * mSystemData.launchAngleHorizDeg * 0.5f;
    const f32v2 newVel = MathUtil::RotateVector(mInitialVelocity.x, mInitialVelocity.y, angleDeg);

    particle.mVelocity.x = newVel.x;
    particle.mVelocity.y = newVel.y;
    particle.mVelocity.z = mInitialVelocity.z;
    
    particle.mSize = (ui16)(getRandomValFromRange(mSystemData.particleSizeRange) * PARTICLE_SIZE_SCALE);
    // Lifetime offsets
    const float thisLifetime = getRandomValFromRange(mSystemData.particleLifetimeSecondsRange);
    particle.mLifeTime = mSystemData.particleLifetimeSecondsRange.y - thisLifetime;
}

float ParticleSystem::getRandomValFromRange(const f32v2& range)
{
    return vmath::lerp(range.x, range.y, Random::getCachedRandomf());
}

int ParticleSystem::getRandomValFromRange(const i32v2& range)
{
    const int modulo = range.y - range.x + 1;
    return range.x + (Random::getCachedRandom() % modulo);
}
