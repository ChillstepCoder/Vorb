#pragma once

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "util/ArbitraryObjectArray.h"
#include "CPUParticleEmitterModule.h"
#include "ParticleEnumTypes.h"

#include "rendering/particle/ParticleSystemInputs.h"
#include "rendering/particle/ParticleEmitterVariableName.h"

class CPUParticleSystem;
class MaterialShaderDef;
class ParticleEmitterDef;


struct CpuParticlesGpuData {
    std::unique_ptr<GpuStreamingDataBuffer> mPositionsBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mRotationsBuffer;
    //std::unique_ptr<GpuStreamingDataBuffer> mVelocitiesBuffer; // Velocities are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mScalesBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mColorsBuffer; // Shared as HDR or non HDR
    //std::unique_ptr<GpuStreamingDataBuffer> mLifespansBuffer; // Lifespans are not needed on the GPU
    std::unique_ptr<GpuStreamingDataBuffer> mMaterialsBuffer;
};
static_assert(e_cast(ParticleComponentType::TERM) == 65);

struct CPUParticlesData {
    // We cannot point these to the GpuStreamingDataBuffer stores because we need coherent info from the previous frame
    // So we will memcopy every frame into the streaming buffers
    // 
    // Components
    std::unique_ptr<f32v3[]> mPositions; // If position.x == FLT_MAX, then particle is inactive
    std::unique_ptr<f32v2[]> mRotations;
    std::unique_ptr<f32v3[]> mVelocities;
    std::unique_ptr<f32v2[]> mScales;
    std::unique_ptr<color4[]> mColors;
    std::unique_ptr<f32v4[]> mHDRColors;
    std::unique_ptr<f32[]> mLifetimes;
    std::unique_ptr<f32[]> mLifespans;
    std::unique_ptr<ui32[]> mMaterials;

    // TODO: Vector is likely faster for small data set, profile
    // Variables
    FlatMap<ParticleEmitterVariableNameUInt, std::unique_ptr<ui32[]>> mUIntVariables;
    FlatMap<ParticleEmitterVariableNameFloat, std::unique_ptr<f32[]>> mFloatVariables;
    FlatMap<ParticleEmitterVariableNameVec2, std::unique_ptr<f32v2[]>> mVec2Variables;
    FlatMap<ParticleEmitterVariableNameVec3, std::unique_ptr<f32v3[]>> mVec3Variables;

    // Optional streaming buffers to push to GPU
    FlatMap<ParticleEmitterVariableNameUInt, std::unique_ptr<GpuStreamingDataBuffer>> mUIntVariableBuffers;
    FlatMap<ParticleEmitterVariableNameFloat, std::unique_ptr<GpuStreamingDataBuffer>> mFloatVariableBuffers;
    FlatMap<ParticleEmitterVariableNameVec2, std::unique_ptr<GpuStreamingDataBuffer>> mVec2VariableBuffers;
    FlatMap<ParticleEmitterVariableNameVec3, std::unique_ptr<GpuStreamingDataBuffer>> mVec3VariableBuffers;
};
static_assert(e_cast(ParticleComponentType::TERM) == 65);

typedef std::function<void(class CpuParticleEmitter& emitter, CPUParticlesData& particleData, f32 elapsedSec)> ParticleUpdateFunction;

class CpuParticleEmitter {
public:
    CpuParticleEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, ParticleSystemInputs* inputs, f32 lifetime = FLT_MAX);
    CpuParticleEmitter(const ParticleEmitterDef& def, ParticleSystemInputs* inputs);
    ~CpuParticleEmitter();

    VORB_NON_COPYABLE(CpuParticleEmitter);

    POOLED_ALLOC_DECL();

    // Bind shader before calling this.
    // Returns true once lifetime has expired
    bool update(f32 elapsedSec);

    // Expects shader to be already bound
    void render();

    // Particles
    ParticleID tryAddParticle(f32v3 position);
    void removeParticle(ParticleID id);
    // Mutators
    void setParticlePosition(ParticleID id, f32v3 position) noexcept;
    void setParticleScale(ParticleID id, f32v2 scale) noexcept;
    void multiplyParticleScale(ParticleID id, f32v2 scale) noexcept;
    void setParticleVelocity(ParticleID id, f32v3 velocity) noexcept;
    void addParticleVelocity(ParticleID id, f32v3 velocity) noexcept;
    void multiplyParticleVelocity(ParticleID id, f32v3 scale) noexcept;
    void setParticleColor(ParticleID id, color4 color) noexcept;
    void setParticleHDRColor(ParticleID id, f32v4 color) noexcept;
    void setParticleMaterial(ParticleID id, MaterialID material) noexcept;
    void setParticleRotation(ParticleID id, f32v2 rollPitch) noexcept;
    void setParticleLifespan(ParticleID id, f32 lifespan) noexcept;
    // Accessors
    f32v3 getParticlePosition(ParticleID id) const noexcept { return mParticleData.mPositions[id]; }
    f32v2 getParticleScale(ParticleID id) const noexcept { return mParticleData.mScales[id]; }
    f32v3 getParticleVelocity(ParticleID id) const noexcept { return mParticleData.mVelocities[id]; }
    color4 getParticleColor(ParticleID id) const noexcept { return mParticleData.mColors[id]; }
    f32v4 getParticleHDRColor(ParticleID id) const noexcept { return mParticleData.mHDRColors[id]; }
    f32v2 getParticleRotation(ParticleID id) const noexcept { return mParticleData.mRotations[id]; }
    f32 getParticleNormalizedLifetime(ParticleID id) const noexcept;
    MaterialID getParticleMaterial(ParticleID id) const noexcept { return mParticleData.mMaterials[id]; }
    CPUParticlesData& getParticleData() { return mParticleData; }

    // Variables
    uint getUIntVariable(ParticleEmitterVariableNameUInt name, ParticleID id) const;
    f32 getFloatVariable(ParticleEmitterVariableNameFloat name, ParticleID id) const;
    f32v2 getVec2Variable(ParticleEmitterVariableNameVec2 name, ParticleID id) const;
    f32v3 getVec3Variable(ParticleEmitterVariableNameVec3 name, ParticleID id) const;

    void setUIntVariable(ParticleEmitterVariableNameUInt name, ParticleID id, uint value);
    void setFloatVariable(ParticleEmitterVariableNameFloat name, ParticleID id, f32 value);
    void setVec2Variable(ParticleEmitterVariableNameVec2 name, ParticleID id, f32v2 value);
    void setVec3Variable(ParticleEmitterVariableNameVec3 name, ParticleID id, f32v3 value);

    bool hasUIntVariable(ParticleEmitterVariableNameUInt name) const;
    bool hasFloatVariable(ParticleEmitterVariableNameFloat name) const;
    bool hasVec2Variable(ParticleEmitterVariableNameVec2 name) const;
    bool hasVec3Variable(ParticleEmitterVariableNameVec3 name) const;

    // Inputs
    const ParticleSystemInputs& getInputs() const { return *mInputs; }

    // Global state
    void setGlobalParticleScale(f32v2 scale) noexcept { mGlobalParticleScale = scale; }
    f32v2 getGlobalParticleScale() const noexcept { return mGlobalParticleScale; }
    void setGlobalParticleColor(color4 color) noexcept { mGlobalParticleColor = color; }
    color4 getGlobalParticleColor() const noexcept { return mGlobalParticleColor; }
    void setGlobalMaterialID(MaterialID materialID) noexcept;
    MaterialID getGlobalMaterialID() const noexcept { return mGlobalMaterialID; }
    void setGlobalParticleLifespan(f32 lifespan) noexcept { mGlobalParticleLifespan = lifespan; }
    f32 getGlobalParticleLifespan() const noexcept { return mGlobalParticleLifespan; }

    int getFirstActiveParticle() const noexcept { return mFirstActiveParticle; }
    int getLastActiveParticle() const noexcept { return mLastActiveParticle; }
    int getNumActiveParticles() const noexcept { return mNumActiveParticles; }
    int getFragmentation() const noexcept { return mNumActiveParticles ? ((mLastActiveParticle - mFirstActiveParticle) / mNumActiveParticles) : 0; }

    f32 getTotalElapsedSec() const noexcept { return mTotalElapsedSec; }
    bool isLooping() const noexcept { return mLooping; }

    AssetID getShaderID() const noexcept { return mShaderID; }

    void setBlendMode(ParticleBlendMode blendMode) noexcept { mBlendMode = blendMode; }
    ParticleBlendMode getBlendMode() const noexcept { return mBlendMode; }

    // Modules
    template <typename T> requires std::derived_from<T, CPUParticleEmitterModule>
    void addEmitterUpdateModule(const T& module) {
        assert(module.getStages().isBitSet(ParticleEmitterModuleStage::EmitterUpdate));
        assert(mNumEmitterUpdateMethods == mEmitterModuleMethods.size() && "All emitter update methods must be added first");
        module.addModuleDataToArray(mParticleModuleData);
        mEmitterModuleMethods.emplace_back(module.getMethod());
        ++mNumEmitterUpdateMethods;
    }

    template <typename T> requires std::derived_from<T, CPUParticleEmitterModule>
    void addParticleInitModule(const T& module) {
        assert(module.getStages().isBitSet(ParticleEmitterModuleStage::ParticleInit));
        assert(mNumParticleInitMethods + mNumEmitterUpdateMethods == mEmitterModuleMethods.size() && "All particle init methods must be added second");
        module.addModuleDataToArray(mParticleModuleData);
        mEmitterModuleMethods.emplace_back(module.getMethod());
        ++mNumParticleInitMethods;
    }

    template <typename T> requires std::derived_from<T, CPUParticleEmitterModule>
    void addParticleUpdateModule(const T& module) {
        assert(module.getStages().isBitSet(ParticleEmitterModuleStage::ParticleUpdate));
        module.addModuleDataToArray(mParticleModuleData);
        // Add last
        mEmitterModuleMethods.emplace_back(module.getMethod());
    }

    void emitParticles(ui32v2 countRange);
    void emitParticles(int count);
protected:
    void allocateParticleData();
    void onNewParticleAdded(ParticleID id);

    // Updates the whole emitter with custom logic.
    // Can be null which implies static system, such as for UI
    ParticleUpdateFunction mNativeUpdateFunction;

    // Contiguous storage of particle module data for efficient iteration
    ArbitraryObjectArray mParticleModuleData;
    std::vector<CPUParticleEmitterModuleMethod> mEmitterModuleMethods;
    int mNumEmitterUpdateMethods = 0;
    int mNumParticleInitMethods = 0;

    CPUParticlesData mParticleData;
    CpuParticlesGpuData mGpuData;
    std::vector<ParticleID> mFreeParticleIDs;

    // Global data
    f32v2 mGlobalParticleScale = f32v2(1.0f);
    f32 mGlobalParticleLifespan = 3.0f;
    color4 mGlobalParticleColor = color::White;
    MaterialID mGlobalMaterialID = INVALID_MATERIAL_ID;
    std::unique_ptr<AssetHandleBundle> mMaterialAssetHandles;
    int mFirstActiveParticle = 0;
    int mLastActiveParticle = -1;
    int mNumActiveParticles = 0;
    int mMaxParticles;
    bool mLooping = false;
    bool mNeedsFindFirstParticle = false;
    bool mNeedsFindLastParticle = false;
    ParticleBlendMode mBlendMode = ParticleBlendMode::Additive;
    // Inputs
    ParticleSystemInputs* mInputs = nullptr;

    UnorderedFlatSet<MaterialID> mContainedMaterials;

    AssetID mShaderID;

    f32 mTotalElapsedSec = 0.0f;
    f32 mLifetimeSec;
    f32 mLastElapsedSec = 0.0f;

    // Determines which data streams we will use
    BitFlags<ParticleComponentType> mComponents;
    std::vector<ParticleEmitterVariableNameVec3> mVec3Variables;
};

// Helper utility that binds depth and blend states
extern void bindStateForParticleBlendMode(ParticleBlendMode blendMode);