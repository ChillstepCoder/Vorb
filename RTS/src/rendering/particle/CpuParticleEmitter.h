#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "util/ArbitraryObjectArray.h"

#include "CPUParticleEmitterModule.h"
#include "ParticleComponentType.h"

class CPUParticleSystem;
class MaterialShader;


struct CpuParticlesGpuData {
    std::unique_ptr<GpuStreamingDataBuffer> mPositionsAndRotationsBuffer;
    //std::unique_ptr<GpuStreamingDataBuffer> mVelocitiesBuffer; // Velocities are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mScalesBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mColorsBuffer; // Shared as HDR or non HDR
    //std::unique_ptr<GpuStreamingDataBuffer> mLifespansBuffer; // Lifespans are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mMaterialsBuffer;
};
static_assert(e_cast(ParticleComponentType::TERM) == 65);

struct CPUParticlesData {
    std::unique_ptr<f32v3[]> mPositions; // If position.x == FLT_MAX, then particle is inactive
    std::unique_ptr<f32[]> mRotations;
    std::unique_ptr<f32v3[]> mVelocities;
    std::unique_ptr<f32v2[]> mScales;
    std::unique_ptr<color4[]> mColors;
    std::unique_ptr<f32v4[]> mHDRColors;
    std::unique_ptr<f32[]> mLifespans;
    std::unique_ptr<ui32[]> mMaterials;
};
static_assert(e_cast(ParticleComponentType::TERM) == 65);

typedef std::function<void(class CpuParticleEmitter& emitter, CPUParticlesData& particleData, f32 elapsedSec)> ParticleUpdateFunction;

struct EmitterSpawnBurst {
    ui32v2 mEmitCountRange = ui32v2(1, 1);
};

struct EmitterSpawnPeriodic {
    f32v2 mEmitRateRangeSec = f32v2(0.0f, 0.2f);
    f32 mNextEmitTime = 0.0f;
    ui32v2 mEmitCountRange = ui32v2(1, 1);
};

class CpuParticleEmitter {
public:
    CpuParticleEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 lifetime = FLT_MAX);
    ~CpuParticleEmitter();

    VORB_NON_COPYABLE(CpuParticleEmitter);

    // Bind shader before calling this.
    // Returns true once lifetime has expired
    bool updateAndRender(f32 elapsedSec);

    // Particles
    ParticleID tryAddParticle(f32v3 position);
    void removeParticle(ParticleID id);
    // Mutators
    void setParticlePosition(ParticleID id, f32v3 position);
    void setParticleScale(ParticleID id, f32v2 scale);
    void setParticleVelocity(ParticleID id, f32v3 velocity);
    void setParticleColor(ParticleID id, color4 color);
    void setParticleHDRColor(ParticleID id, f32v4 color);
    void setParticleMaterial(ParticleID id, MaterialID material);
    // Accessors
    f32v3 getParticlePosition(ParticleID id) const { return mParticleData.mPositions[id]; }
    f32v2 getParticleScale(ParticleID id) const { return mParticleData.mScales[id]; }
    f32v3 getParticleVelocity(ParticleID id) const { return mParticleData.mVelocities[id]; }
    color4 getParticleColor(ParticleID id) const { return mParticleData.mColors[id]; }
    f32v4 getParticleHDRColor(ParticleID id) const { return mParticleData.mHDRColors[id]; }
    MaterialID getParticleMaterial(ParticleID id) const { return mParticleData.mMaterials[id]; }
    CPUParticlesData& getParticleData() { return mParticleData; }

    void markDataChanged() { mDataChanged = true; }

    // Global state
    void setGlobalParticleScale(f32v2 scale) { mGlobalParticleScale = scale; }
    f32v2 getGlobalParticleScale() const { return mGlobalParticleScale; }
    void setGlobalParticleColor(color4 color) { mGlobalParticleColor = color; }
    color4 getGlobalParticleColor() const { return mGlobalParticleColor; }
    void setGlobalMaterialID(MaterialID materialID) { assert(!mParticleData.mMaterials); mGlobalMaterialID = materialID; }
    MaterialID getGlobalMaterialID() const { assert(!mParticleData.mMaterials); return mGlobalMaterialID; }

    ui32 getFirstActiveParticle() const { return mFirstActiveParticle; }
    ui32 getLastActiveParticle() const { return mLastActiveParticle; }

    f32 getTotalElapsedSec() const { return mTotalElapsedSec; }

    const MaterialShader& getMaterialShader() const { return mShader; }

    // Modules
    template <typename T> requires std::derived_from<T, CPUParticleEmitterModule>
    void addParticleInitModule(const T& module) {
        assert(mNumParticleInitMethods == mEmitterModuleMethods.size() && "All init methods must be added first");
        module.addModuleDataToArray(mParticleModuleData);
        mEmitterModuleMethods.emplace_back(module.getMethod());
        ++mNumParticleInitMethods;
    }

    template <typename T> requires std::derived_from<T, CPUParticleEmitterModule>
    void addParticleUpdateModule(const T& module) {
        module.addModuleDataToArray(mParticleModuleData);
        mEmitterModuleMethods.emplace_back(module.getMethod());
    }


protected:
    void updateSpawning();
    void emitParticles(ui32v2 countRange);
    void render();
    void onNewParticleAdded(ParticleID id);

    const MaterialShader& mShader;

    // Updates the whole emitter with custom logic.
    // Can be null which implies static system, such as for UI
    ParticleUpdateFunction mNativeUpdateFunction;

    // Contiguous storage of particle module data for efficient iteration
    ArbitraryObjectArray mParticleModuleData;
    std::vector<CPUParticleEmitterModuleMethod> mEmitterModuleMethods;
    int mNumParticleInitMethods = 0;

    CPUParticlesData mParticleData;
    CpuParticlesGpuData mGpuData;
    std::vector<ParticleID> mFreeParticleIDs;

    // Global data
    f32v2 mGlobalParticleScale = f32v2(1.0f);
    color4 mGlobalParticleColor = color::White;
    MaterialID mGlobalMaterialID = 0;
    ui32 mFirstActiveParticle = 0;
    ui32 mLastActiveParticle = 0;
    ui32 mActiveParticles = 0;
    ui32 mMaxParticles;
    int mBaseInstance = 0;
    bool mDataChanged = false;
    bool mNeedsFindFirstParticle = false;
    bool mNeedsFindLastParticle = false;

    f32 mTotalElapsedSec = 0.0f;
    f32 mLifetimeSec;

    // Determines which data streams we will use
    BitFlags<ParticleComponentType> mComponents;

    std::variant<EmitterSpawnBurst, EmitterSpawnPeriodic> mSpawnData;
    TimePoint mLastEmittedParticleTime = TimePoint::min();
};