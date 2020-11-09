#include "stdafx.h"
#include "ParticleSystem.h"

#include "Random.h"

ParticleSystem::ParticleSystem(const f32v3& rootPosition, const f32v3& initialVelocity, const ParticleSystemData& systemData) :
    mParticles(systemData.maxParticles),
    mRootPosition(rootPosition),
    mSystemData(systemData)
{
    // TODO: Would be nice to have RAII wrapper around buffer objects
    glGenBuffers(1, &mVbo);
    glGetIntegerv(GL_POINT_SIZE_RANGE, &mMaxPixelSize);

    initParticles();
}

ParticleSystem::~ParticleSystem()
{
    glDeleteBuffers(1, &mVbo);
}

bool ParticleSystem::update(float deltaTime)
{
    if (mSystemData.isEmitter) {
        tryEmitParticles(deltaTime);
    }
    
    // Motion
    for (auto&& particle : mParticles) {
        // x, y, z = 16 bit velocity  w
        particle.mPosition += particle.mVelocity * deltaTime;
        particle.mVelocity *= mSystemData.airDamping;
        // Bounce off the ground
        if (particle.mPosition.z <= 0.0f) {
            particle.mPosition.x *= mSystemData.hitDamping;
            particle.mPosition.y *= mSystemData.hitDamping;
            particle.mPosition.z = -particle.mPosition.z * mSystemData.hitDamping;
            // TODO: Generate collision event
        }
    }

    // TODO: Custom logic
    switch (mSystemData.type) {
        case ParticleSystemType::SIMPLE:
            break;
        case ParticleSystemType::FIRE:
            break;
    }

    return mActiveParticles == 0;
}

void ParticleSystem::initParticles()
{
    for (auto&& particle : mParticles) {
        emitParticle(particle);
    }
}

void ParticleSystem::tryEmitParticles(float deltaTime)
{
    mEmitTimer -= deltaTime;
    while (mEmitTimer < 0.0f) {
        mEmitTimer += mEmitRate;
        if (mActiveParticles < mParticles.size()) {
            emitParticle(mParticles[mActiveParticles++]);
        }
    }
}

inline void ParticleSystem::emitParticle(Particle& particle) {
    // TODO: Random emit
    particle.mPosition = mRootPosition;
    particle.mVelocity = mInitialVelocity;
}
