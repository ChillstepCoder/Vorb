#include "stdafx.h"
#include "CpuParticleEmitter.h"

#include "rendering/particle/CpuParticleEmitter.h"
#include "rendering/MaterialShaderDef.h"
#include "rendering/MaterialShaderRepository.h"

#include "resources/MaterialRepository.h"
#include "resources/asset/AssetHandleBundle.h"

#include "definitions/ParticleSystemDef.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>

#include "math/Random.h"

// Arbitrary for estimated perf reasons
constexpr ui32 MAX_PARTICLES = 20000;

POOLED_ALLOC_DEF_NOT_THREADSAFE(CpuParticleEmitter, 64, ASSERT_RENDER_THREAD());

void bindStateForParticleBlendMode(ParticleBlendMode blendMode) {

    // TODO: Switch to using RenderDevice
    switch (blendMode) {
        case ParticleBlendMode::Opaque:
            vg::DepthState::FULL.set();
            vg::sBlendStates.REPLACE.set();
            break;
        case ParticleBlendMode::Alpha:
            vg::DepthState::READ.set();
            vg::sBlendStates.ALPHA.set();
            break;
        case ParticleBlendMode::Additive:
            vg::DepthState::READ.set();
            vg::sBlendStates.ADDITIVE.set();
            break;
        case ParticleBlendMode::Subtractive:
            vg::DepthState::READ.set();
            vg::sBlendStates.SUBTRACTIVE.set();
            break;
    }
}

CpuParticleEmitter::CpuParticleEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, ParticleSystemInputs* inputs, f32 lifetime /*= FLT_MAX*/) :
    mShaderID(shader.getID()),
    mNativeUpdateFunction(updateFunction),
    mMaxParticles(maxParticles),
    mComponents(components),
    mLifetimeSec(lifetime),
    mInputs(inputs),
    mMaterialAssetHandles(std::make_unique<AssetHandleBundle>())
{
    assert(mInputs);
    allocateParticleData();
}

CpuParticleEmitter::CpuParticleEmitter(const ParticleEmitterDef& def, ParticleSystemInputs* inputs) :
    mShaderID(def.mShaderRef.getAssetID()),
    mInputs(inputs),
    mMaterialAssetHandles(std::make_unique<AssetHandleBundle>())
{
    assert(inputs);
    mMaxParticles = def.mMaxParticles;
    mGlobalParticleScale = def.mDefaultScale;
    mGlobalParticleColor = def.mDefaultColor;
    mGlobalParticleLifespan = def.mDefaultParticleLifespanSec;
    mGlobalMaterialID = def.mDefaultMaterialID;
    if (mGlobalMaterialID != INVALID_MATERIAL_ID) {
        mContainedMaterials.insert(mGlobalMaterialID);
        mMaterialAssetHandles->addAssetHandle(MaterialRepository::get().getAssetHandle(mGlobalMaterialID));
    }
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

bool CpuParticleEmitter::update(f32 elapsedSec) {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();

    mLastElapsedSec = elapsedSec;
    mTotalElapsedSec += elapsedSec;

    const bool lifetimeExpired = mTotalElapsedSec >= mLifetimeSec;

    if (mNativeUpdateFunction) {
        mNativeUpdateFunction(*this, mParticleData, elapsedSec);
    }

    if (!lifetimeExpired) { // Update emitter only if we arent expired
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

    return lifetimeExpired && (mNumActiveParticles <= 0);
}

void CpuParticleEmitter::render() {

    PROFILE_FUNCTION();
    if (mNumActiveParticles == 0) {
        return;
    }
    const MaterialShaderDef& shader = MaterialShaderRepository::get().getLoadedAsset(mShaderID);
    const vg::GLProgram& program = shader.mProgram;

    // TODO: UBO?
    const VGUniform unIsUsingColor = program.getUniform("unIsUsingColor");
    const VGUniform unIsUsingHDRColor = program.getUniform("unIsUsingHDRColor");
    const VGUniform unIsUsingMaterial = program.getUniform("unIsUsingMaterial");
    const VGUniform unIsUsingScale = program.getUniform("unIsUsingScale");
    const VGUniform unIsUsingRotation = program.getUniform("unIsUsingRotation");
    const VGUniform unGlobalColor = program.getUniform("unGlobalColor");
    const VGUniform unGlobalScale = program.getUniform("unGlobalScale");
    const VGUniform unGlobalMaterial = program.getUniform("unGlobalMaterial");

    // Upload globals
    glUniform4f(unGlobalColor,
        mGlobalParticleColor.r / 255.0f,
        mGlobalParticleColor.g / 255.0f,
        mGlobalParticleColor.b / 255.0f,
        mGlobalParticleColor.a / 255.0f);
    glUniform2fv(unGlobalScale, 1, &mGlobalParticleScale.x);

    // Find first and last particles so we ensure we are drawing the minimum number of elements
    // TODO: profile this as we could use bit array for faster testing?
    if (mNeedsFindFirstParticle) {
        PROFILE_SCOPE("Find First Particle");
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

    // Positions + padding float
    f32v4* positions = (f32v4*)mGpuData.mPositionsBuffer->frameBeginAndGetDataForUpdate();
    for (ui32 i = 0; i < particlesToRender; ++i) {
        const ui32 particleIndex = mFirstActiveParticle + i;
        const f32v3& sourcePos = mParticleData.mPositions[particleIndex];
        positions[i] = f32v4(sourcePos.x, sourcePos.y, sourcePos.z, 0.0f /*Rotation??*/);
    }
    mGpuData.mPositionsBuffer->flushDataAndIncrementFrame(particlesToRender);
    mGpuData.mPositionsBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_POSITIONS_SSBO);

    // Rotations
    if (mParticleData.mRotations) {
        f32v2* rotations = (f32v2*)mGpuData.mRotationsBuffer->frameBeginAndGetDataForUpdate();
        memcpy(rotations, &mParticleData.mRotations[mFirstActiveParticle], particlesToRender * sizeof(f32v2));
        mGpuData.mRotationsBuffer->flushDataAndIncrementFrame(particlesToRender);
        mGpuData.mRotationsBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_ROTATIONS_SSBO);
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
        mGpuData.mScalesBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_SCALES_SSBO);
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
            mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_HDR_COLORS_SSBO);
            glUniform1ui(unIsUsingHDRColor, 1u);
            glUniform1ui(unIsUsingColor, 0u);
        }
        else {
            color4* colors = (color4*)mGpuData.mColorsBuffer->frameBeginAndGetDataForUpdate();
            memcpy(colors, &mParticleData.mColors[mFirstActiveParticle], sizeof(color4) * particlesToRender);
            mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender);
            mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_COLORS_SSBO);
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
        mGpuData.mMaterialsBuffer->bindBufferAsSSBO(BUFFER_BASE_PARTICLE_MATERIALS_SSBO);
        glUniform1ui(unIsUsingMaterial, 1u);
    }
    else {
        glUniform1ui(unGlobalMaterial, (GLuint)mGlobalMaterialID);
        glUniform1ui(unIsUsingMaterial, 0u);
    }

    // Render two triangles per particle with no vertex data
    {
        PROFILE_SCOPE("Draw");
        sGlobalFullTriangleVAO.drawNTriangles(particlesToRender * 2);
    }

    checkGlError("CpuParticleEmitter::render");
    return;
}


ParticleID CpuParticleEmitter::tryAddParticle(f32v3 position) {
    if (mNumActiveParticles >= mMaxParticles) {
        return INVALID_PARTICLE_ID;
    }

    ParticleID newId;
    if (mFreeParticleIDs.size()) {
        newId = mFreeParticleIDs.back();
        mFreeParticleIDs.pop_back();
    }
    else {
        newId = mNumActiveParticles;
    }
    onNewParticleAdded(newId);
    // This function always overrides position TODO: IS this what we always want? It will ignore modules
    mParticleData.mPositions[newId] = position;
    return newId;
}

void CpuParticleEmitter::removeParticle(ParticleID id) {
    assert(mNumActiveParticles > 0);

    if (--mNumActiveParticles == 0) {
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
}

void CpuParticleEmitter::setParticleScale(ParticleID id, f32v2 scale) {
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] = scale;
}

void CpuParticleEmitter::multiplyParticleScale(ParticleID id, f32v2 scale) {
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] *= scale;
}

void CpuParticleEmitter::setParticleVelocity(ParticleID id, f32v3 velocity) {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] = velocity;
}

void CpuParticleEmitter::addParticleVelocity(ParticleID id, f32v3 velocity) {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] += velocity;
}

void CpuParticleEmitter::multiplyParticleVelocity(ParticleID id, f32v3 scale) {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] *= scale;
}

void CpuParticleEmitter::setParticleColor(ParticleID id, color4 color) {
    assert(mComponents.isBitSet(ParticleComponentType::Color));
    mParticleData.mColors[id] = color;
}

void CpuParticleEmitter::setParticleHDRColor(ParticleID id, f32v4 color) {
    assert(mComponents.isBitSet(ParticleComponentType::HDRColor));
    mParticleData.mHDRColors[id] = color;
}

void CpuParticleEmitter::setParticleMaterial(ParticleID id, MaterialID material) {
    assert(mComponents.isBitSet(ParticleComponentType::MaterialID));
    mParticleData.mMaterials[id] = (ui32)material;
    // TODO: can we preload these instead of doing it here?
    if (mContainedMaterials.find(material) == mContainedMaterials.end()) {
        mContainedMaterials.insert(material);
        mMaterialAssetHandles->addAssetHandle(MaterialRepository::get().getAssetHandle(material));
    }
}

void CpuParticleEmitter::setParticleRotation(ParticleID id, f32v2 rollPitch) {
    assert(mComponents.isBitSet(ParticleComponentType::Rotation));
    mParticleData.mRotations[id].x = rollPitch.x;
    mParticleData.mRotations[id].y = rollPitch.y;
}

void CpuParticleEmitter::setParticleLifespan(ParticleID id, f32 lifespan) {
    assert(mComponents.isBitSet(ParticleComponentType::Lifespan));
    mParticleData.mLifespans[id] = lifespan;
}

f32 CpuParticleEmitter::getParticleNormalizedLifetime(ParticleID id) const {
    if (mParticleData.mLifespans) {
        return glm::min(mParticleData.mLifetimes[id] / mParticleData.mLifespans[id], 1.0f);
    }
    return glm::min(mParticleData.mLifetimes[id] / mGlobalParticleLifespan, 1.0f);
}

void CpuParticleEmitter::setGlobalMaterialID(MaterialID materialID)
{
    mGlobalMaterialID = materialID;
    if (mGlobalMaterialID != INVALID_MATERIAL_ID) {
        mContainedMaterials.insert(mGlobalMaterialID);
        mMaterialAssetHandles->addAssetHandle(MaterialRepository::get().getAssetHandle(materialID));
    }
}

void CpuParticleEmitter::emitParticles(ui32v2 countRange) {
    
    int emitCount = (int)(Random::getCachedRandom() % (countRange.y - countRange.x)) + countRange.x;
    emitCount = glm::min(emitCount, mNumActiveParticles - mMaxParticles);
    emitParticles(emitCount);
}

void CpuParticleEmitter::emitParticles(int count) {
    if (count == 0) {
        return;
    }
    assert(mNumActiveParticles <= mMaxParticles);
    count = glm::min(count, mMaxParticles - mNumActiveParticles);
    for (int i = 0; i < count; ++i) {
        if (mFreeParticleIDs.size()) {
            ParticleID recycledId = mFreeParticleIDs.back();
            mParticleData.mPositions[recycledId] = f32v3(0.0f);
            mFreeParticleIDs.pop_back();
            onNewParticleAdded(recycledId);
        }
        else {
            onNewParticleAdded(mNumActiveParticles);
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


    // Add all variable data
    for (auto& uintVar : mParticleData.mUIntVariables) {
        uintVar.second = std::make_unique_for_overwrite<ui32[]>(mMaxParticles);
    }
    for (auto& floatVar : mParticleData.mFloatVariables) {
        floatVar.second = std::make_unique_for_overwrite<f32[]>(mMaxParticles);
    }
    for (auto& vec2Var : mParticleData.mVec2Variables) {
        vec2Var.second = std::make_unique_for_overwrite<f32v2[]>(mMaxParticles);
    }
    for (auto& vec3Var : mParticleData.mVec3Variables) {
        vec3Var.second = std::make_unique_for_overwrite<f32v3[]>(mMaxParticles);
    }

    static_assert(e_cast(ParticleComponentType::TERM) == 65);
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

    ++mNumActiveParticles;
    if (mNumActiveParticles == 1) {
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
