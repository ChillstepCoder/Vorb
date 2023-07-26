#include "stdafx.h"
#include "CpuParticleEmitter.h"

#include "rendering/particle/CpuParticleEmitter.h"
#include "rendering/MaterialShader.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/GLProgram.h>

#include "math/Random.h"

// Arbitrary for estimated perf reasons
constexpr ui32 MAX_PARTICLES = 20000;

CpuParticleEmitter::CpuParticleEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 lifetime /*= FLT_MAX*/) :
    mShader(shader),
    mNativeUpdateFunction(updateFunction),
    mMaxParticles(maxParticles),
    mComponents(components),
    mLifetimeSec(lifetime)
{
    ASSERT_RENDER_THREAD();
    assert(mMaxParticles <= MAX_PARTICLES);

    mParticleData.mPositions = std::unique_ptr<f32v3[]>(new f32v3[mMaxParticles]);
    // These must be f32v4 to match std430 layout, so we always have rotation allocated on GPU even
    // if we arent using it (rotation is w component)
    mGpuData.mPositionsAndRotationsBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(f32v4));


    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities = std::unique_ptr<f32v3[]>(new f32v3[mMaxParticles]);
        std::memset(mParticleData.mVelocities.get(), 0, mMaxParticles * sizeof(f32v3)); // Default values
        // No GPU data for velocities
        // TODO: VelocityCPU vs VelocityGPU
    }
    if (mComponents.isBitSet(ParticleComponentType::Scale)) {
        mParticleData.mScales = std::unique_ptr<f32v2[]>(new f32v2[mMaxParticles]);
        mGpuData.mScalesBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(f32v2));
        std::fill_n(mParticleData.mScales.get(), mMaxParticles, f32v2(1.0f)); // Default values
    }
    if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
        mParticleData.mHDRColors = std::unique_ptr<f32v4[]>(new f32v4[mMaxParticles]);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(f32v4));
    }
    else if (mComponents.isBitSet(ParticleComponentType::Color)) {
        mParticleData.mColors = std::unique_ptr<color4[]>(new color4[mMaxParticles]);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(color4));
    }
    if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
        mParticleData.mLifespans = std::unique_ptr<f32[]>(new f32[mMaxParticles]);
        // No GPU data for lifespans
    }
    if (mComponents.isBitSet(ParticleComponentType::MaterialID)) {
        mParticleData.mMaterials = std::unique_ptr<ui32[]>(new ui32[mMaxParticles]);
        mGpuData.mMaterialsBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(ui32));
    }
    if (mComponents.isBitSet(ParticleComponentType::Rotation)) {
        mParticleData.mRotations = std::unique_ptr<f32[]>(new f32[mMaxParticles]);
        std::memset(mParticleData.mRotations.get(), 0, mMaxParticles * sizeof(f32)); // Default values
        // Rotations are packed into mPositionsAndRotationsBuffer which is always allocated
    }

    static_assert(e_cast(ParticleComponentType::TERM) == 65);

    // Burst spawning
    if (std::holds_alternative<EmitterSpawnBurst>(mSpawnData)) {
        emitParticles(std::get<EmitterSpawnBurst>(mSpawnData).mEmitCountRange);
    }
}

CpuParticleEmitter::~CpuParticleEmitter() {
}

bool CpuParticleEmitter::updateAndRender(f32 elapsedSec) {
    ASSERT_RENDER_THREAD();

    mTotalElapsedSec += elapsedSec;

    // If we dont have an update function, we are a static system or manually updated system
    // which only needs to update GPU data on particle add or remove or manual flag dirty
    if (mNativeUpdateFunction) {
        mDataChanged = true;
        mNativeUpdateFunction(*this, mParticleData, elapsedSec);
    }

    // Run every particle through the modules
    // TODO: Multithreaded with triple buffer state?
    for (ui32 i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
        // TODO: Profile probability?
        if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] {
            continue;
        }
        for (size_t j = mNumParticleInitMethods; j < mEmitterModuleMethods.size(); ++j) {
            mEmitterModuleMethods[j](*this, i, mParticleModuleData[j]);
        }
    }

    render();

    return mTotalElapsedSec >= mLifetimeSec;
}

ParticleID CpuParticleEmitter::tryAddParticle(f32v3 position) {
    if (mActiveParticles >= mMaxParticles) {
        return INVALID_PARTICLE_ID;
    }

    mDataChanged = true;

    if (mFreeParticleIDs.size()) {
        ParticleID recycledId = mFreeParticleIDs.back();
        mParticleData.mPositions[recycledId] = position;
        mFreeParticleIDs.pop_back();
        onNewParticleAdded(recycledId);
        return recycledId;
    }
    mParticleData.mPositions[mActiveParticles] = position;
    onNewParticleAdded(mActiveParticles);
    return mActiveParticles - 1;
}

void CpuParticleEmitter::removeParticle(ParticleID id) {
    mDataChanged = true;
    assert(mActiveParticles > 0);

    if (--mActiveParticles == 0) {
        mFirstActiveParticle = mLastActiveParticle = 0;
    }
    else {
        if (id == mFirstActiveParticle) {
            mNeedsFindFirstParticle = true;
        }
        else if (id == mLastActiveParticle) {
            mNeedsFindLastParticle = true;
        }
    }

    // This indicates we were already removed
    assert(mParticleData.mPositions[id].x != FLT_MAX);
    // Shove particle off the screen
    mParticleData.mPositions[id] = f32v3(FLT_MAX);
    // If we are scaling, scale to zero for good measure
    if (mParticleData.mScales) {
        mParticleData.mScales[id] = f32v2(0.0f);
    }
    mFreeParticleIDs.emplace_back(id);
}

void CpuParticleEmitter::setParticlePosition(ParticleID id, f32v3 position) {
    mParticleData.mPositions[id] = position;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleScale(ParticleID id, f32v2 scale) {
    mParticleData.mScales[id] = scale;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleVelocity(ParticleID id, f32v3 velocity) {
    mParticleData.mVelocities[id] = velocity;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleColor(ParticleID id, color4 color) {
    assert(mComponents.isBitSet(ParticleComponentType::Color));
    mParticleData.mColors[id] = color;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleHDRColor(ParticleID id, f32v4 color) {
    assert(mComponents.isBitSet(ParticleComponentType::HDRColor));
    mParticleData.mHDRColors[id] = color;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleMaterial(ParticleID id, MaterialID material) {
    mParticleData.mMaterials[id] = (ui32)material;
    mDataChanged = true;
}

void CpuParticleEmitter::fillVariableFromType(CPUParticleEmitterVariable& variable, ParticleID id, CPUParticleEmitterVariableType type) {
    assert(type >= CPUParticleEmitterVariableType::BUILTINS_BEGIN);
    
    switch (type) {
        case CPUParticleEmitterVariableType::Position:
            variable.mVarData = mParticleData.mPositions.get()[id];
            break;
        case CPUParticleEmitterVariableType::Velocity:
            variable.mVarData = mParticleData.mVelocities.get()[id];
            break;
        case CPUParticleEmitterVariableType::Scale:
            variable.mVarData = mParticleData.mScales.get()[id];
            break;
        case CPUParticleEmitterVariableType::Color:
            variable.mVarData = mParticleData.mColors.get()[id];
            break;
        case CPUParticleEmitterVariableType::HDRColor:
            variable.mVarData = mParticleData.mHDRColors.get()[id];
            break;
        case CPUParticleEmitterVariableType::Lifespan:
            variable.mVarData = mParticleData.mLifespans.get()[id];
            break;
        case CPUParticleEmitterVariableType::Rotation:
            variable.mVarData = mParticleData.mRotations.get()[id];
            break;
    }

    static_assert(e_count(CPUParticleEmitterVariableType) == 10);
}

void CpuParticleEmitter::updateSpawning() {
    if (std::holds_alternative<EmitterSpawnPeriodic>(mSpawnData)) {
        EmitterSpawnPeriodic& spawnData = std::get<EmitterSpawnPeriodic>(mSpawnData);
        const f32 timeDiff = mTotalElapsedSec - spawnData.mNextEmitTime;
        if (timeDiff >= 0.0f) {
            emitParticles(spawnData.mEmitCountRange);
            spawnData.mNextEmitTime = mTotalElapsedSec + Random::getCachedRandomf() * (spawnData.mEmitRateRangeSec.y - spawnData.mEmitRateRangeSec.x) + spawnData.mEmitRateRangeSec.x - timeDiff;
        }
    }
}

void CpuParticleEmitter::emitParticles(ui32v2 countRange) {

    assert(mActiveParticles <= mMaxParticles);

    ui32 emitCount = ((ui32)Random::getCachedRandom() % (countRange.y - countRange.x)) + countRange.x;
    emitCount = glm::min(emitCount, mActiveParticles - mMaxParticles);
    if (emitCount == 0) {
        return;
    }

    mDataChanged = true;
    for (ui32 i = 0; i < emitCount; ++i) {
        if (mFreeParticleIDs.size()) {
            ParticleID recycledId = mFreeParticleIDs.back();
            mParticleData.mPositions[recycledId] = f32v3(0.0f);
            mFreeParticleIDs.pop_back();
            onNewParticleAdded(recycledId);
        }
        mParticleData.mPositions[mActiveParticles] = f32v3(0.0f);
        onNewParticleAdded(mActiveParticles);
    }
}

void CpuParticleEmitter::render() {

    if (mActiveParticles == 0) {
        return;
    }

    vg::GLProgram& program = mShader.mProgram;

    // TODO: UBO?
    const VGUniform unIsUsingColor = program.getUniform("unIsUsingColor");
    const VGUniform unIsUsingHDRColor = program.getUniform("unIsUsingHDRColor");
    const VGUniform unIsUsingMaterial = program.getUniform("unIsUsingMaterial");
    const VGUniform unIsUsingScale = program.getUniform("unIsUsingScale");

    // Upload globals
    glUniform4f(program.getUniform("unGlobalColor"),
        mGlobalParticleColor.r / 255.0f,
        mGlobalParticleColor.g / 255.0f,
        mGlobalParticleColor.b / 255.0f,
        mGlobalParticleColor.a / 255.0f);
    glUniform2fv(program.getUniform("unGlobalScale"), 1, &mGlobalParticleScale.x);

    // Find first and last particles so we ensure we are drawing the minimum number of elements
    // TODO: profile this as we could use bit array for faster testing?
    if (mNeedsFindFirstParticle) {
        for (ui32 i = mFirstActiveParticle + 1; i < mMaxParticles; ++i) {
            if (mParticleData.mPositions[i].x != FLT_MAX) {
                mFirstActiveParticle = i;
                break;
            }
        }
        mNeedsFindFirstParticle = false;
    }
    if (mNeedsFindLastParticle) {
        for (int i = (int)mLastActiveParticle - 1; i >= 0; --i) {
            if (mParticleData.mPositions[i].x != FLT_MAX) {
                mLastActiveParticle = i;
                break;
            }
        }
        mNeedsFindLastParticle = false;
    }

    const ui32 particlesToRender = (mLastActiveParticle - mFirstActiveParticle) + 1;

    // Always bind positions
    mGpuData.mPositionsAndRotationsBuffer->bindBufferAsSSBO(BUFFER_BASE_POSITIONS_SSBO);

    if (mDataChanged) {
        mDataChanged = false;
        // Positions
        f32v4* positionsAndRotations = (f32v4*)mGpuData.mPositionsAndRotationsBuffer->frameBeginAndGetDataForUpdate();
        if (mParticleData.mRotations) {
            for (ui32 i = 0; i < particlesToRender; ++i) {
                const ui32 particleIndex = mFirstActiveParticle + i;
                const f32v3& sourcePos = mParticleData.mPositions[particleIndex];
                positionsAndRotations[i] = f32v4(sourcePos.x, sourcePos.y, sourcePos.z, mParticleData.mRotations[particleIndex]);
            }
        }
        else {
            for (ui32 i = 0; i < particlesToRender; ++i) {
                const f32v3& sourcePos = mParticleData.mPositions[mFirstActiveParticle + i];
                positionsAndRotations[i] = f32v4(sourcePos.x, sourcePos.y, sourcePos.z, 0.0f);
            }
        }
        mBaseInstance = mGpuData.mPositionsAndRotationsBuffer->flushDataAndIncrementFrame(particlesToRender);

        // Scales
        if (mGpuData.mScalesBuffer) {
            f32v2* scales = (f32v2*)mGpuData.mScalesBuffer->frameBeginAndGetDataForUpdate();
            memcpy(scales, &mParticleData.mScales[mFirstActiveParticle], sizeof(f32v2) * particlesToRender);
            assert(mBaseInstance == mGpuData.mScalesBuffer->flushDataAndIncrementFrame(particlesToRender));
            mGpuData.mScalesBuffer->bindBufferAsSSBO(BUFFER_BASE_SCALES_SSBO);
            glUniform1ui(unIsUsingScale, 1u);
        }
        else {
            glUniform1ui(unIsUsingScale, 0u);
        }

        // Colors
        if (mGpuData.mColorsBuffer) {
            if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
                f32v4* colors = (f32v4*)mGpuData.mColorsBuffer->frameBeginAndGetDataForUpdate();
                memcpy(colors, &mParticleData.mHDRColors[mFirstActiveParticle], sizeof(f32v4) * particlesToRender);
                assert(mBaseInstance == mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender));
                mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_HDR_COLORS_SSBO);
                glUniform1ui(unIsUsingHDRColor, 1u);
                glUniform1ui(unIsUsingColor, 0u);
            }
            else {
                color4* colors = (color4*)mGpuData.mColorsBuffer->frameBeginAndGetDataForUpdate();
                memcpy(colors, &mParticleData.mColors[mFirstActiveParticle], sizeof(color4) * particlesToRender);
                assert(mBaseInstance == mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender));
                mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_COLORS_SSBO);
                glUniform1ui(unIsUsingHDRColor, 0u);
                glUniform1ui(unIsUsingColor, 1u);
            }
        }
        else {
            glUniform1ui(unIsUsingColor, 0u);
            glUniform1ui(unIsUsingHDRColor, 0u);
        }

        // Materials
        if (mGpuData.mMaterialsBuffer) {
            ui32* materials = (ui32*)mGpuData.mMaterialsBuffer->frameBeginAndGetDataForUpdate();
            memcpy(materials, &mParticleData.mMaterials[mFirstActiveParticle], sizeof(ui32) * particlesToRender);
            assert(mBaseInstance == mGpuData.mMaterialsBuffer->flushDataAndIncrementFrame(particlesToRender));
            mGpuData.mMaterialsBuffer->bindBufferAsSSBO(BUFFER_BASE_MATERIALS_SSBO);
            glUniform1ui(unIsUsingMaterial, 1u);
        }
        else {
            glUniform1ui(program.getUniform("unGlobalMaterial"), (GLuint)mGlobalMaterialID);
            glUniform1ui(unIsUsingMaterial, 0u);
        }
    }
    else {
        // Scales
        if (mGpuData.mScalesBuffer) {
            mGpuData.mScalesBuffer->bindBufferAsSSBO(BUFFER_BASE_SCALES_SSBO);
            glUniform1ui(unIsUsingScale, 1u);
        }
        else {
            glUniform1ui(unIsUsingScale, 0u);
        }

        // Colors
        if (mGpuData.mColorsBuffer) {
            if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
                mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_HDR_COLORS_SSBO);
                glUniform1ui(unIsUsingHDRColor, 1u);
                glUniform1ui(unIsUsingColor, 0u);
            }
            else {
                mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_COLORS_SSBO);
                glUniform1ui(unIsUsingHDRColor, 0u);
                glUniform1ui(unIsUsingColor, 1u);
            }
        }
        else {
            glUniform1ui(unIsUsingColor, 0u);
            glUniform1ui(unIsUsingHDRColor, 0u);
        }

        // Materials
        if (mGpuData.mMaterialsBuffer) {
            mGpuData.mMaterialsBuffer->bindBufferAsSSBO(BUFFER_BASE_MATERIALS_SSBO);
            glUniform1ui(unIsUsingMaterial, 1u);
        }
        else {
            glUniform1ui(program.getUniform("unGlobalMaterial"), (GLuint)mGlobalMaterialID);
            glUniform1ui(unIsUsingMaterial, 0u);
        }
    }
    static_assert(e_cast(ParticleComponentType::TERM) == 65);

    glUniform1ui(program.getUniform("unBaseInstanceOffset"), mBaseInstance);

    // Render two triangles per particle with no vertex data
    sGlobalFullTriangleVAO.drawNTriangles(particlesToRender * 2);
}

void CpuParticleEmitter::onNewParticleAdded(ParticleID id) {

    // Init particle
    for (int i = 0; i < mNumParticleInitMethods; ++i) {
        mEmitterModuleMethods[i](*this, id, mParticleModuleData[i]);
    }

    ++mActiveParticles;
    if (mActiveParticles == 1) {
        mFirstActiveParticle = mLastActiveParticle = id;
        mNeedsFindFirstParticle = mNeedsFindLastParticle = false;
    }
    else {
        if (id <= mFirstActiveParticle) {
            mFirstActiveParticle = id;
            mNeedsFindFirstParticle = false;
        }
        else if (id >= mLastActiveParticle) {
            mLastActiveParticle = id;
            mNeedsFindLastParticle = false;
        }
    }
}
