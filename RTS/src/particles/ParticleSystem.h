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
    ParticleSystem(const f32v3& rootPosition, const f32v3& direction, const ParticleSystemData& systemData);
    ~ParticleSystem();

    // Returns true on empty
    bool update(float deltaTime, const f32v2& playerPos);

    void setRootPosition(const f32v3& rootPosition) { mRootPosition = rootPosition; }
    void sestDirection(const f32v3& direction) { mDirection = direction; }

    void stop();

protected:
    void tryEmitParticles();
    void emitParticle();
    float getRandomValFromRange(const f32v2& range);
    int getRandomValFromRange(const i32v2& range);

    f32v3 mRootPosition;
    f32v3 mDirection;
    ParticleSystemData mSystemData;
    // TODO: Recycle memory
    std::vector<Particle> mParticles;
    f32 mEmitTimer = 0.0f;
    f32 mEmitRate = 0.0f;
    f32 mCurrentTime = 0.0f;
    f32 mTotalDuration = 0.0f;
    bool mIsStopped = false;
    // Mesh data used by ParticleSystemRenderer
    mutable VGBuffer mVbo = 0;
    mutable VGBuffer mVao = 0;
    mutable bool mDirty = true; // Set this any time you want mesh to update
};

