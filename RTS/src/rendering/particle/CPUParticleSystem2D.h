#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"

// Position is implicit
enum class ParticleComponentType : ui8 {
    Velocity = BIT(0),
    Scale = BIT(1),
    Color = BIT(2),
    Lifespan = BIT(3),
    MaterialID = BIT(4),
    // TODO: SortDepth?
    TERM
};

struct CPUParticleSystemData2D {
    std::unique_ptr<f32v3[]> mPositions; // If position.x == FLT_MAX, then particle is inactive
    std::unique_ptr<f32v3[]> mVelocities;
    std::unique_ptr<f32v2[]> mScales;
    std::unique_ptr<color4[]> mColors;
    std::unique_ptr<f32[]> mLifespans;
    std::unique_ptr<ui32[]> mMaterials;
};
static_assert(e_cast(ParticleComponentType::TERM) == 17);

struct CpuParticleSystemGpuData2D {
    std::unique_ptr<GpuStreamingDataBuffer> mPositionsBuffer;
    //std::unique_ptr<GpuStreamingDataBuffer> mVelocitiesBuffer; // Velocities are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mScalesBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mColorsBuffer;
    //std::unique_ptr<GpuStreamingDataBuffer> mLifespansBuffer; // Lifespans are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mMaterialsBuffer;
};
static_assert(e_cast(ParticleComponentType::TERM) == 17);

typedef std::function<void(class CPUParticleSystem2D& system, CPUParticleSystemData2D& particleData, f32 elapsedSec)> ParticleUpdateFunction;

// Versatile 2D quad rendering system used for both particles and simple UI
class CPUParticleSystem2D
{
public:
    CPUParticleSystem2D(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components);

    VORB_NON_COPYABLE(CPUParticleSystem2D);

    // Bind shader before calling this
    void updateAndRender(f32 elapsedSec);

    // Particles
    ParticleID tryAddParticle(f32v3 position);
    void removeParticle(ParticleID id);
    void setParticlePosition(ParticleID id, f32v3 position);
    void setParticleScale(ParticleID id, f32v2 scale);
    void setParticleVelocity(ParticleID id, f32v3 velocity);
    void setParticleColor(ParticleID id, color4 color);
    void setParticleMaterial(ParticleID id, MaterialID material);
    CPUParticleSystemData2D& getParticleData() { return mParticleData; }
    
    void markDataChanged() { mDataChanged = true; }

    // Global state
    void setGlobalParticleScale(f32v2 scale) { mGlobalParticleScale = scale; }
    f32v2 getGlobalParticleScale() const { return mGlobalParticleScale; }
    void setGlobalParticleColor(color4 color) { mGlobalParticleColor = color; }
    color4 getGlobalParticleColor() const { return mGlobalParticleColor; }

    f32 getTotalElapsedSec() const { return mTotalElapsedSec; }

private:
    void render();
    void onNewParticleAdded(ParticleID id);

    // Updates the whole system with custom logic.
    // Can be null which implies static system, such as for UI
    ParticleUpdateFunction mUpdateFunction;

    CPUParticleSystemData2D mParticleData;
    CpuParticleSystemGpuData2D mGpuData;
    std::vector<ParticleID> mFreeParticleIDs;

    // Global data
    f32v2 mGlobalParticleScale = f32v2(1.0f);
    color4 mGlobalParticleColor = color::White;
    ui32 mFirstActiveParticle = 0;
    ui32 mLastActiveParticle = 0;
    ui32 mActiveParticles = 0;
    ui32 mMaxParticles;
    int mCurrentUniformBufferStartIndex = 0;
    bool mDataChanged = false;
    bool mNeedsFindFirstParticle = false;
    bool mNeedsFindLastParticle = false;

    f32 mTotalElapsedSec = 0.0f;

    // Determines which data streams we will use
    BitFlags<ParticleComponentType> mComponents;
};
