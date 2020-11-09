#pragma once

#include "ParticleSystemData.h"

class Camera2D;

// A particle is also vertex data, and is streamed to the GPU. It is both data and rendering
struct Particle {
    f32v3 mPosition;
    f32v3 mVelocity;
    f32 mLifeTime;
    ui16 mSize; // 1 / PARTICLE_SIZE_SCALE scale
    ui8 mData[2];
};
constexpr unsigned PARTICLE_SIZE_SCALE = 1024;
static_assert(sizeof(Particle) == 32, "Power of 2 byte alignment needed");

class ParticleSystem
{
    friend class ParticleSystemRenderer;
public:
    ParticleSystem(const f32v3& rootPosition, const f32v3& initialVelocity, const ParticleSystemData& systemData);
    ~ParticleSystem();

    // Returns true on empty
    bool update(float deltaTime);

    void setRootPosition(const f32v3& rootPosition) { mRootPosition = rootPosition; }
    void setInitialVelocity(const f32v3& initialVelocity) { mInitialVelocity = initialVelocity; }

protected:
    void initParticles();
    void tryEmitParticles(float deltaTime);
    void emitParticle(Particle& particle);

    GLint mMaxPixelSize;
    f32v3 mRootPosition;
    f32v3 mInitialVelocity;
    ParticleSystemData mSystemData;
    // TODO: Recycle memory
    VGBuffer mVbo = 0;
    std::vector<Particle> mParticles;
    unsigned mActiveParticles = 0;
    f32 mEmitTimer = 0.0f;
    f32 mEmitRate = 1.0f;
    // Used by ParticleSystemRenderer, set every time you update
    mutable bool mDirty = true;
};

