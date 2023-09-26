#include "stdafx.h"
#include "CpuParticleEmitter.h"

#include "rendering/particle/CpuParticleEmitter.h"
#include "rendering/MaterialShaderDef.h"

#include "definitions/ParticleSystemDef.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>

#include "math/Random.h"

// Arbitrary for estimated perf reasons
constexpr ui32 MAX_PARTICLES = 20000;

CpuParticleEmitter::CpuParticleEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, f32 lifetime /*= FLT_MAX*/) :
    mShader(shader),
    mNativeUpdateFunction(updateFunction),
    mMaxParticles(maxParticles),
    mComponents(components),
    mLifetimeSec(lifetime)
{
    allocateParticleData();

}

CpuParticleEmitter::CpuParticleEmitter(const ParticleEmitterDef& def) : mShader(def.mShader->getLoadedAsset()) {

    mMaxParticles = def.mMaxParticles;
    mGlobalParticleScale = def.mDefaultScale;
    mGlobalParticleColor = def.mDefaultColor;
    mGlobalParticleLifespan = def.mDefaultParticleLifespanSec;
    mGlobalMaterialID = def.mDefaultMaterialID;
    mLifetimeSec = def.mLifetimeSec;
    mLooping = def.mLooping;
    mBlendMode = def.mBlendMode;

    const size_t totalModuleCount =
        def.mModules.mEmitterUpdate.size() +
        def.mModules.mParticleInit.size() +
        def.mModules.mParticleUpdate.size();
    mEmitterModuleMethods.reserve(totalModuleCount);

    for (auto&& module : def.mModules.mEmitterUpdate) {
        addEmitterUpdateModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    for (auto&& module : def.mModules.mParticleInit) {
        addParticleInitModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    for (auto&& module : def.mModules.mParticleUpdate) {
        addParticleUpdateModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    allocateParticleData();
}

CpuParticleEmitter::~CpuParticleEmitter() {
}

bool CpuParticleEmitter::updateAndRender(f32 elapsedSec) {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();

    mLastElapsedSec = elapsedSec;
    mTotalElapsedSec += elapsedSec;

    if (mNativeUpdateFunction) {
        mDataChanged = true;
        mNativeUpdateFunction(*this, mParticleData, elapsedSec);
    }

    {// Update emitter
        PROFILE_SCOPE("Emitter Update Methods");
        for (size_t i = 0; i < mNumEmitterUpdateMethods; ++i) {
            mEmitterModuleMethods[i](*this, INVALID_PARTICLE_ID, mParticleModuleData[i], elapsedSec);
        }
    }

    // Run every particle through the modules
    // TODO: Multithreaded with triple buffer state?

#define UPDATE_LOGIC \
    for (size_t j = mNumEmitterUpdateMethods + mNumParticleInitMethods; j < mEmitterModuleMethods.size(); ++j) { \
        mEmitterModuleMethods[j](*this, i, mParticleModuleData[j], elapsedSec); \
    }

    { // Split out to avoid branching in critical path
        PROFILE_SCOPE("Particle Update Methods");
        if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
            if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
                mDataChanged = true;

                for (int i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
                    if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] {
                        continue;
                    }
                    mParticleData.mLifetimes[i] += elapsedSec;
                    if (mParticleData.mLifetimes[i] >= mParticleData.mLifespans[i]) {
                        removeParticle(i);
                    }
                    else {
                        UPDATE_LOGIC;
                        mParticleData.mPositions[i] += elapsedSec * mParticleData.mVelocities[i];
                    }
                }
            }
            else {
                for (int i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
                    if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] {
                        continue;
                    }
                    mParticleData.mLifetimes[i] += elapsedSec;
                    if (mParticleData.mLifetimes[i] >= mParticleData.mLifespans[i]) {
                        removeParticle(i);
                    }
                    else {
                        UPDATE_LOGIC;
                    }
                }
            }
        }
        else {
            if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
                mDataChanged = true;

                for (int i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
                    if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] {
                        continue;
                    }
                    mParticleData.mLifetimes[i] += elapsedSec;
                    if (mParticleData.mLifetimes[i] >= mGlobalParticleLifespan) {
                        removeParticle(i);
                    }
                    else {
                        UPDATE_LOGIC;
                        mParticleData.mPositions[i] += elapsedSec * mParticleData.mVelocities[i];
                    }
                }
            }
            else {
                for (int i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
                    if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] {
                        continue;
                    }
                    mParticleData.mLifetimes[i] += elapsedSec;
                    if (mParticleData.mLifetimes[i] >= mGlobalParticleLifespan) {
                        removeParticle(i);
                    }
                    else {
                        UPDATE_LOGIC;
                    }
                }
            }
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

    ParticleID newId;
    if (mFreeParticleIDs.size()) {
        newId = mFreeParticleIDs.back();
        mFreeParticleIDs.pop_back();
    }
    else {
        newId = mActiveParticles;
    }
    onNewParticleAdded(newId);
    // This function always overrides position TODO: IS this what we always want? It will ignore modules
    mParticleData.mPositions[newId] = position;
    return newId;
}

void CpuParticleEmitter::removeParticle(ParticleID id) {
    mDataChanged = true;
    assert(mActiveParticles > 0);

    if (--mActiveParticles == 0) {
        mFirstActiveParticle = 0;
        mLastActiveParticle = -1;
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
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] = scale;
    mDataChanged = true;
}

void CpuParticleEmitter::multiplyParticleScale(ParticleID id, f32v2 scale) {
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] *= scale;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleVelocity(ParticleID id, f32v3 velocity) {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] = velocity;
    mDataChanged = true;
}

void CpuParticleEmitter::addParticleVelocity(ParticleID id, f32v3 velocity) {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] += velocity;
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
    assert(mComponents.isBitSet(ParticleComponentType::MaterialID));
    mParticleData.mMaterials[id] = (ui32)material;
    mDataChanged = true;
}

void CpuParticleEmitter::setParticleRotation(ParticleID id, f32v2 rollPitch) {
    assert(mComponents.isBitSet(ParticleComponentType::Rotation));
    mParticleData.mRotations[id].x = rollPitch.x;
    mParticleData.mRotations[id].y = rollPitch.y;
    mDataChanged = true;
}

f32 CpuParticleEmitter::getParticleNormalizedLifetime(ParticleID id) const {
    if (mParticleData.mLifespans) {
        return glm::min(mParticleData.mLifetimes[id] / mParticleData.mLifespans[id], 1.0f);
    }
    return glm::min(mParticleData.mLifetimes[id] / mGlobalParticleLifespan, 1.0f);
}

void CpuParticleEmitter::emitParticles(ui32v2 countRange) {
    
    int emitCount = (int)(Random::getCachedRandom() % (countRange.y - countRange.x)) + countRange.x;
    emitCount = glm::min(emitCount, mActiveParticles - mMaxParticles);
    emitParticles(emitCount);
}

void CpuParticleEmitter::emitParticles(int count) {
    if (count == 0) {
        return;
    }
    assert(mActiveParticles <= mMaxParticles);
    count = glm::min(count, mMaxParticles - mActiveParticles);
    mDataChanged = true;
    for (int i = 0; i < count; ++i) {
        if (mFreeParticleIDs.size()) {
            ParticleID recycledId = mFreeParticleIDs.back();
            mParticleData.mPositions[recycledId] = f32v3(0.0f);
            mFreeParticleIDs.pop_back();
            onNewParticleAdded(recycledId);
        }
        else {
            onNewParticleAdded(mActiveParticles);
        }
    }
}

void CpuParticleEmitter::allocateParticleData()
{
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();
    assert(mMaxParticles <= MAX_PARTICLES);
    
    // Arbitrary
    mFreeParticleIDs.reserve(mMaxParticles / 4);

    mParticleData.mPositions = std::make_unique_for_overwrite<f32v3[]>(mMaxParticles);
    mParticleData.mLifetimes = std::make_unique_for_overwrite<f32[]>(mMaxParticles);
    // These must be f32v4 to match std430 layout, so we have a padding float right now
    mGpuData.mPositionsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v4));

    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities = std::make_unique_for_overwrite<f32v3[]>(mMaxParticles);
        // No GPU data for velocities
        // TODO: VelocityCPU vs VelocityGPU
    }
    if (mComponents.isBitSet(ParticleComponentType::Rotation)) {
        mParticleData.mRotations = std::make_unique_for_overwrite<f32v2[]>(mMaxParticles);
        mGpuData.mRotationsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v2));
    }
    if (mComponents.isBitSet(ParticleComponentType::Scale)) {
        mParticleData.mScales = std::make_unique_for_overwrite<f32v2[]>(mMaxParticles);
        mGpuData.mScalesBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v2));
    }
    if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
        mParticleData.mHDRColors = std::make_unique_for_overwrite<f32v4[]>(mMaxParticles);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v4));
    }
    else if (mComponents.isBitSet(ParticleComponentType::Color)) {
        mParticleData.mColors = std::make_unique_for_overwrite<color4[]>(mMaxParticles);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(color4));
    }
    if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
        mParticleData.mLifespans = std::make_unique_for_overwrite<f32[]>(mMaxParticles);
        // No GPU data for lifespans
    }
    if (mComponents.isBitSet(ParticleComponentType::MaterialID)) {
        mParticleData.mMaterials = std::make_unique_for_overwrite<ui32[]>(mMaxParticles);
        mGpuData.mMaterialsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(ui32));
    }

    static_assert(e_cast(ParticleComponentType::TERM) == 65);
}

void CpuParticleEmitter::render() {

    PROFILE_FUNCTION();
    if (mActiveParticles == 0) {
        return;
    }

    vg::GLProgram& program = mShader.mProgram;

    // TODO: UBO?
    const VGUniform unIsUsingColor = program.getUniform("unIsUsingColor");
    const VGUniform unIsUsingHDRColor = program.getUniform("unIsUsingHDRColor");
    const VGUniform unIsUsingMaterial = program.getUniform("unIsUsingMaterial");
    const VGUniform unIsUsingScale = program.getUniform("unIsUsingScale");
    const VGUniform unIsUsingRotation = program.getUniform("unIsUsingRotation");

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
        PROFILE_SCOPE("Find Last Particle");
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
    mGpuData.mPositionsBuffer->bindBufferAsSSBO(BUFFER_BASE_POSITIONS_SSBO);
    if (mDataChanged) {
        mDataChanged = false;
        // Positions + padding float
        f32v4* positions = (f32v4*)mGpuData.mPositionsBuffer->frameBeginAndGetDataForUpdate();
        for (ui32 i = 0; i < particlesToRender; ++i) {
            const ui32 particleIndex = mFirstActiveParticle + i;
            const f32v3& sourcePos = mParticleData.mPositions[particleIndex];
            positions[i] = f32v4(sourcePos.x, sourcePos.y, sourcePos.z, 0.0f /*Rotation??*/);
        }
        mBaseInstance = mGpuData.mPositionsBuffer->flushDataAndIncrementFrame(particlesToRender);

        // Rotations
        if (mParticleData.mRotations) {
            f32v2* rotations = (f32v2*)mGpuData.mRotationsBuffer->frameBeginAndGetDataForUpdate();
            memcpy(rotations, &mParticleData.mRotations[mFirstActiveParticle], particlesToRender * sizeof(f32v2));
            mGpuData.mRotationsBuffer->flushDataAndIncrementFrame(particlesToRender);
            mGpuData.mRotationsBuffer->bindBufferAsSSBO(BUFFER_BASE_ROTATIONS_SSBO);
            glUniform1ui(unIsUsingRotation, 1u);
        }
        else {
            glUniform1ui(unIsUsingRotation, 0u);
        }

        // Scales
        if (mGpuData.mScalesBuffer) {
            f32v2* scales = (f32v2*)mGpuData.mScalesBuffer->frameBeginAndGetDataForUpdate();
            memcpy(scales, &mParticleData.mScales[mFirstActiveParticle], sizeof(f32v2) * particlesToRender);
            mGpuData.mScalesBuffer->flushDataAndIncrementFrame(particlesToRender);
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
                mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender);
                mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_HDR_COLORS_SSBO);
                glUniform1ui(unIsUsingHDRColor, 1u);
                glUniform1ui(unIsUsingColor, 0u);
            }
            else {
                color4* colors = (color4*)mGpuData.mColorsBuffer->frameBeginAndGetDataForUpdate();
                memcpy(colors, &mParticleData.mColors[mFirstActiveParticle], sizeof(color4) * particlesToRender);
                mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender);
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
            mGpuData.mMaterialsBuffer->flushDataAndIncrementFrame(particlesToRender);
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

    vg::DepthState::READ.set();
    switch (mBlendMode) {
        case ParticleBlendMode::Additive:
            vg::BlendState::set(vorb::graphics::BlendStateType::ADDITIVE);
            break;
        case ParticleBlendMode::Subtractive:
            vg::BlendState::set(vorb::graphics::BlendStateType::SUBTRACTIVE);
            break;
        case ParticleBlendMode::Alpha:
            vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);
            break;
        default:
            break;
    }
    static_assert(e_count(ParticleBlendMode) == 3);

    // Render two triangles per particle with no vertex data
    sGlobalFullTriangleVAO.drawNTriangles(particlesToRender * 2);

    checkGlError("CpuParticleEmitter::render");

    vg::DepthState::restorePrevious();
    vg::BlendState::restorePrevious();
}

void CpuParticleEmitter::onNewParticleAdded(ParticleID id) {

    // Zero data
    mParticleData.mPositions[id] = f32v3(0.0f);
    mParticleData.mLifetimes[id] = 0.0f;
    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities[id] = f32v3(0.0f);
    }
    if (mComponents.isBitSet(ParticleComponentType::Scale)) {
        mParticleData.mScales[id] = f32v2(1.0f);
    }
    if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
        mParticleData.mHDRColors[id] = f32v4(1.0f);
    }
    else if (mComponents.isBitSet(ParticleComponentType::Color)) {
        mParticleData.mColors[id] = color::White;
    }
    if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
        mParticleData.mLifespans[id] = 1.0f;
    }
    if (mComponents.isBitSet(ParticleComponentType::MaterialID)) {
        mParticleData.mMaterials[id] = 0;
    }
    if (mComponents.isBitSet(ParticleComponentType::Rotation)) {
        mParticleData.mRotations[id] = f32v2(0.0f);
    }

    // Init particle
    for (int i = mNumEmitterUpdateMethods; i < mNumEmitterUpdateMethods + mNumParticleInitMethods; ++i) {
        mEmitterModuleMethods[i](*this, id, mParticleModuleData[i], mLastElapsedSec);
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
