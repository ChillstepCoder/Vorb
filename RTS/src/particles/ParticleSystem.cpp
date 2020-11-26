#include "stdafx.h"
#include "ParticleSystem.h"

#include <glm/gtx/rotate_vector.hpp>

#include "Random.h"

const float GRAVITY_CONSTANT = 0.01f;

const f32 PLAYER_ABSORB_HEIGHT = 0.9f;
const f32 PARTICLE_ABSORB_DISTANCE = 0.2f;
const f32 FORCE_DISTANCE_INNNER = 0.1f;
const f32 FORCE_DISTANCE_OUTER = 5.5f;
const f32 FORCE_DISTANCE_SPAN = FORCE_DISTANCE_OUTER - FORCE_DISTANCE_INNNER;
const float FORCE_POWER_MULT = 0.015f;

ParticleSystem::ParticleSystem(const f32v3& rootPosition, const f32v3& initialVelocity, const ParticleSystemData& systemData) :
    mRootPosition(rootPosition),
    mSystemData(systemData),
    mDirection(initialVelocity)
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

bool ParticleSystem::update(float deltaTime, const f32v2& playerPos)
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
        // Player force field influence
#define USE_FORCE 1
#if USE_FORCE == 1
        {
            const f32v2 offsetToPlayer = playerPos - reinterpret_cast<f32v2&>(particle.mPosition);
            const f32 distanceToPlayer = glm::length(offsetToPlayer);
            if (distanceToPlayer < PARTICLE_ABSORB_DISTANCE && particle.mPosition.z < PLAYER_ABSORB_HEIGHT) {
                particle = mParticles.back();
                mParticles.pop_back();
                continue;
            }
            const f32v2 offsetToPlayerNormalized = offsetToPlayer / distanceToPlayer;

            f32v3 forceInner(0.0f, 0.0f, 1.0f);
            f32v3 forceOuter(offsetToPlayerNormalized.x, offsetToPlayerNormalized.y, 0.0f);

            const f32 forceLerp = (1.0f - vmath::clamp((distanceToPlayer - FORCE_DISTANCE_INNNER) / FORCE_DISTANCE_SPAN, 0.0f, 1.0f));
            const f32 heightMult = vmath::max(1.0f - particle.mPosition.z, 0.0f);
            const f32 totalForce = (forceLerp * FORCE_POWER_MULT);
            const f32v3 resultForce = vmath::lerp(forceInner, forceOuter, forceLerp) * totalForce;

            particle.mVelocity += resultForce;
        }
#endif
        // x, y, z = 16 bit velocity w
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
                particle.mPosition.z = 0.001f;
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

    particle.mPosition = mRootPosition;

    { // Determine velocity
        const float horizAngleDeg = ((Random::getCachedRandomf() * 2.0f) - 1.0f) * mSystemData.launchAngleHorizDeg * 0.5f;
        const f32v2 newDir = MathUtil::RotateVector(mDirection.x, mDirection.y, horizAngleDeg);

        // Rotate sideways
        const f32v2 vertRotate = MathUtil::RotateVector(newDir.x, newDir.y, -90.0f);

        particle.mVelocity.x = newDir.x;
        particle.mVelocity.y = newDir.y;
        particle.mVelocity.z = mDirection.z;

        // Rotate up
        const f32 vertRotateDeg = getRandomValFromRange(mSystemData.launchAngleVertRangeDeg);
        particle.mVelocity = glm::rotate(particle.mVelocity, DEG_TO_RAD(vertRotateDeg), glm::normalize(f32v3(vertRotate.x, vertRotate.y, 0.0f)));

        // Scale
        particle.mVelocity *= getRandomValFromRange(mSystemData.speedRange);
    }
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
