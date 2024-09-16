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

// Arbitrary for estimated perf reasons
constexpr ui32 MAX_PARTICLES = 20000;

POOLED_ALLOC_DEF_NOT_THREADSAFE(CpuParticleEmitter, 64, ASSERT_RENDER_THREAD());

CpuParticleEmitter::CpuParticleEmitter(
    const ParticleUpdateFunction& updateFunction,
    ui32 maxParticles,
    BitFlags<ParticleComponentType> components,
    const MaterialShaderDef& shader,
    const ParticleSystemInputs* inputs,
    f32 lifetime /*= FLT_MAX*/
) :
    mShaderID(shader.getID()),
    mNativeUpdateFunction(updateFunction),
    mMaxParticles(maxParticles),
    mComponents(components),
    mLifetimeSec(lifetime),
    mInputs(inputs),
    mMaterialAssetHandles(std::make_unique<AssetHandleBundle>())
{
    allocateComponentData();
}

CpuParticleEmitter::CpuParticleEmitter(
    const ParticleEmitterDef& def,
    const ParticleSystemInputs* inputs,
    const ParticleSystemUserParameterMap* userParameters,
    f32v3 rootPosition,
    const f32m3* systemOrientation,
    const f32m3* inverseSystemOrientation
) :
    mShaderID(def.mShaderRef.getAssetID()),
    mInputs(inputs),
    mUserParameters(userParameters),
    mMaterialAssetHandles(std::make_unique<AssetHandleBundle>()),
    mRootPosition(rootPosition),
    mSystemOrientation(systemOrientation),
    mInverseSystemOrientation(inverseSystemOrientation)
{
    // TODO: some of this information could be cached in the definition to make for faster setup

    mMaxParticles = def.mMaxParticles;
    mGlobalParticleScale = def.mDefaultScale;
    mGlobalParticleColor = def.mDefaultColor;
    mGlobalParticleLifespan = def.mDefaultParticleLifespanSec;
    mGlobalMaterialID = def.mMaterialRef.getAssetID();
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

    // Initialize variables
    mParticleData.mUIntVariables.reserve(def.mUIntVariables.size());
    for (auto uintVar : def.mUIntVariables) {
        mParticleData.mUIntVariables.emplace(uintVar, std::move(std::make_unique_for_overwrite<ui32[]>(mMaxParticles)));
    }
    mParticleData.mFloatVariables.reserve(def.mFloatVariables.size());
    for (auto floatVar : def.mFloatVariables) {
        mParticleData.mFloatVariables.emplace(floatVar, std::move(std::make_unique_for_overwrite<f32[]>(mMaxParticles)));
    }
    mParticleData.mVec2Variables.reserve(def.mVec2Variables.size());
    for (auto vec2Var : def.mVec2Variables) {
        mParticleData.mVec2Variables.emplace(vec2Var, std::move(std::make_unique_for_overwrite<f32v2[]>(mMaxParticles)));
    }
    mParticleData.mVec3Variables.reserve(def.mVec3Variables.size());
    for (auto vec3Var : def.mVec3Variables) {
        mParticleData.mVec3Variables.emplace(vec3Var, std::move(std::make_unique_for_overwrite<f32v3[]>(mMaxParticles)));
    }

    // Allocate shader streaming buffer inputs
    mShaderBindings = std::span<const ParticleEmitterShaderBinding>(def.mShaderBindings.data(), def.mShaderBindings.size());
    for (ParticleEmitterShaderBinding binding : mShaderBindings) {
        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, ParticleEmitterVariableNameUInt>) {
                mParticleData.mUIntVariableBuffers.emplace(val, std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(ui32)));
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameFloat>) {
                mParticleData.mFloatVariableBuffers.emplace(val, std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32)));
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec2>) {
                mParticleData.mVec2VariableBuffers.emplace(val, std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v2)));
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec3>) {
                // f32v4 as we need to align to 16 bytes
                mParticleData.mVec3VariableBuffers.emplace(val, std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v4)));
            }
        }, binding.mVariableName);
        static_assert(std::variant_size_v<ParticleVariableNameVariant> == 4);
    }

    for (auto&& module : def.mModules.mEmitterUpdate) {
        if (module->isValid() && module->compatableWithEmitter(*this)) [[likely]] {
            addEmitterUpdateModule(*module);
            mComponents |= module->getRequiredComponents();
        }
    }

    for (auto&& module : def.mModules.mParticleInit) {
        if (module->isValid() && module->compatableWithEmitter(*this)) [[likely]] {
            addParticleInitModule(*module);
            mComponents |= module->getRequiredComponents();
        }
    }

    for (auto&& module : def.mModules.mParticleUpdate) {
        if (module->isValid() && module->compatableWithEmitter(*this)) [[likely]] {
            addParticleUpdateModule(*module);
            mComponents |= module->getRequiredComponents();
        }
    }

    allocateComponentData();
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
    const VGUniform unGlobalScale = program.getUniform("unGlobalScale");

    // Upload globals
    if (const VGUniform* unGlobalColor = program.tryGetUniform("unGlobalColor")) {
        // This is sometimes not active due to fragment shader optimization
        glUniform4f(*unGlobalColor,
            mGlobalParticleColor.r / 255.0f,
            mGlobalParticleColor.g / 255.0f,
            mGlobalParticleColor.b / 255.0f,
            mGlobalParticleColor.a / 255.0f);
    }
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
        const f32v3 sourcePos = mParticleData.mPositions[particleIndex];
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
        if (const VGUniform* unGlobalMaterial = program.tryGetUniform("unGlobalMaterial")) {
            glUniform1ui(*unGlobalMaterial, (GLuint)mGlobalMaterialID);
        }
        glUniform1ui(unIsUsingMaterial, 0u);
    }

    // Variable bindings
    for (ParticleEmitterShaderBinding binding : mShaderBindings) {
        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            GpuStreamingDataBuffer* buffer;
            if constexpr (std::is_same_v<T, ParticleEmitterVariableNameUInt>) {
                buffer = mParticleData.mUIntVariableBuffers.at(val).get();
                ui32* data = (ui32*)buffer->frameBeginAndGetDataForUpdate();
                memcpy(data, &mParticleData.mUIntVariables.at(val)[mFirstActiveParticle], sizeof(ui32) * particlesToRender);
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameFloat>) {
                buffer = mParticleData.mFloatVariableBuffers.at(val).get();
                f32* data = (f32*)buffer->frameBeginAndGetDataForUpdate();
                memcpy(data, &mParticleData.mFloatVariables.at(val)[mFirstActiveParticle], sizeof(f32) * particlesToRender);
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec2>) {
                buffer = mParticleData.mVec2VariableBuffers.at(val).get();
                f32v2* data = (f32v2*)buffer->frameBeginAndGetDataForUpdate();
                memcpy(data, &mParticleData.mVec2Variables.at(val)[mFirstActiveParticle], sizeof(f32v2) * particlesToRender);
            }
            else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec3>) {
                buffer = mParticleData.mVec3VariableBuffers.at(val).get();
                f32v4* data = (f32v4*)buffer->frameBeginAndGetDataForUpdate();
                const f32v3* sourceData = &mParticleData.mVec3Variables.at(val)[mFirstActiveParticle];
                for (ui32 i = 0; i < particlesToRender; ++i) {
                    const ui32 particleIndex = mFirstActiveParticle + i;
                    const f32v3 s = sourceData[particleIndex];
                    data[i] = f32v4(s.x, s.y, s.z, 0.0f /*Rotation??*/);
                }
            }
            buffer->flushDataAndIncrementFrame(particlesToRender);
            buffer->bindBufferAsSSBO(binding.mShaderBindingIndex);
        }, binding.mVariableName);
        static_assert(std::variant_size_v<ParticleVariableNameVariant> == 4);
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

void CpuParticleEmitter::setParticlePosition(ParticleID id, f32v3 position) noexcept {
    mParticleData.mPositions[id] = position;
}

void CpuParticleEmitter::setParticleScale(ParticleID id, f32v2 scale) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] = scale;
}

void CpuParticleEmitter::multiplyParticleScale(ParticleID id, f32v2 scale) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Scale));
    mParticleData.mScales[id] *= scale;
}

void CpuParticleEmitter::setParticleVelocity(ParticleID id, f32v3 velocity) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] = velocity;
}

void CpuParticleEmitter::addParticleVelocity(ParticleID id, f32v3 velocity) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] += velocity;
}

void CpuParticleEmitter::multiplyParticleVelocity(ParticleID id, f32v3 scale) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Velocity));
    mParticleData.mVelocities[id] *= scale;
}

void CpuParticleEmitter::setParticleColor(ParticleID id, color4 color) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Color));
    mParticleData.mColors[id] = color;
}

void CpuParticleEmitter::setParticleHDRColor(ParticleID id, f32v4 color) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::HDRColor));
    mParticleData.mHDRColors[id] = color;
}

void CpuParticleEmitter::setParticleMaterial(ParticleID id, MaterialID material) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::MaterialID));
    mParticleData.mMaterials[id] = (ui32)material;
    // TODO: can we preload these instead of doing it here?
    if (mContainedMaterials.find(material) == mContainedMaterials.end()) {
        mContainedMaterials.insert(material);
        mMaterialAssetHandles->addAssetHandle(MaterialRepository::get().getAssetHandle(material));
    }
}

void CpuParticleEmitter::setParticleRotation(ParticleID id, f32v2 rollPitch) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Rotation));
    mParticleData.mRotations[id].x = rollPitch.x;
    mParticleData.mRotations[id].y = rollPitch.y;
}

void CpuParticleEmitter::setParticleLifespan(ParticleID id, f32 lifespan) noexcept {
    assert(mComponents.isBitSet(ParticleComponentType::Lifespan));
    mParticleData.mLifespans[id] = lifespan;
}

f32 CpuParticleEmitter::getParticleNormalizedLifetime(ParticleID id) const noexcept {
    if (mParticleData.mLifespans) {
        return glm::min(mParticleData.mLifetimes[id] / mParticleData.mLifespans[id], 1.0f);
    }
    return glm::min(mParticleData.mLifetimes[id] / mGlobalParticleLifespan, 1.0f);
}

uint CpuParticleEmitter::getUIntVariable(ParticleEmitterVariableNameUInt name, ParticleID id) const {
    return assert_at(mParticleData.mUIntVariables, name)[id];
}

f32 CpuParticleEmitter::getFloatVariable(ParticleEmitterVariableNameFloat name, ParticleID id) const {
    return assert_at(mParticleData.mFloatVariables, name)[id];
}

f32v2 CpuParticleEmitter::getVec2Variable(ParticleEmitterVariableNameVec2 name, ParticleID id) const {
    return assert_at(mParticleData.mVec2Variables, name)[id];
}

f32v3 CpuParticleEmitter::getVec3Variable(ParticleEmitterVariableNameVec3 name, ParticleID id) const {
    return assert_at(mParticleData.mVec3Variables, name)[id];
}

void CpuParticleEmitter::setUIntVariable(ParticleEmitterVariableNameUInt name, ParticleID id, uint value) {
    assert_at(mParticleData.mUIntVariables, name)[id] = value;
}

void CpuParticleEmitter::setFloatVariable(ParticleEmitterVariableNameFloat name, ParticleID id, f32 value) {
    assert_at(mParticleData.mFloatVariables, name)[id] = value;
}

void CpuParticleEmitter::setVec2Variable(ParticleEmitterVariableNameVec2 name, ParticleID id, f32v2 value) {
    assert_at(mParticleData.mVec2Variables, name)[id] = value;
}

void CpuParticleEmitter::setVec3Variable(ParticleEmitterVariableNameVec3 name, ParticleID id, f32v3 value) {
    assert_at(mParticleData.mVec3Variables, name)[id] = value;
}

bool CpuParticleEmitter::hasUIntVariable(ParticleEmitterVariableNameUInt name) const {
    return mParticleData.mUIntVariables.find(name) != mParticleData.mUIntVariables.end();
}

bool CpuParticleEmitter::hasFloatVariable(ParticleEmitterVariableNameFloat name) const {
    return mParticleData.mFloatVariables.find(name) != mParticleData.mFloatVariables.end();
}

bool CpuParticleEmitter::hasVec2Variable(ParticleEmitterVariableNameVec2 name) const {
    return mParticleData.mVec2Variables.find(name) != mParticleData.mVec2Variables.end();
}

bool CpuParticleEmitter::hasVec3Variable(ParticleEmitterVariableNameVec3 name) const {
    return mParticleData.mVec3Variables.find(name) != mParticleData.mVec3Variables.end();
}

void CpuParticleEmitter::setGlobalMaterialID(MaterialID materialID) noexcept
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

void CpuParticleEmitter::setAsEditorPreviewEmitter() {
    // Editor emitter can have invalid by default and we dont want to crash while editing
    mParticleData.mUIntVariables[ParticleEmitterVariableNameUInt::INVALID] = std::make_unique_for_overwrite<ui32[]>(mMaxParticles);
    memset(mParticleData.mUIntVariables[ParticleEmitterVariableNameUInt::INVALID].get(), 0, sizeof(ui32) * mMaxParticles);
    mParticleData.mFloatVariables[ParticleEmitterVariableNameFloat::INVALID] = std::make_unique_for_overwrite<f32[]>(mMaxParticles);
    memset(mParticleData.mFloatVariables[ParticleEmitterVariableNameFloat::INVALID].get(), 0, sizeof(f32) * mMaxParticles);
    mParticleData.mVec2Variables[ParticleEmitterVariableNameVec2::INVALID] = std::make_unique_for_overwrite<f32v2[]>(mMaxParticles);
    memset(mParticleData.mVec2Variables[ParticleEmitterVariableNameVec2::INVALID].get(), 0, sizeof(f32v2) * mMaxParticles);
    mParticleData.mVec3Variables[ParticleEmitterVariableNameVec3::INVALID] = std::make_unique_for_overwrite<f32v3[]>(mMaxParticles);
    memset(mParticleData.mVec3Variables[ParticleEmitterVariableNameVec3::INVALID].get(), 0, sizeof(f32v3) * mMaxParticles);
}

void CpuParticleEmitter::allocateComponentData() {
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

void CpuParticleEmitter::onNewParticleAdded(ParticleID id) {

    // Zero data
    mParticleData.mPositions[id] = f32v3(0.0f);
    mParticleData.mLifetimes[id] = 0.0f;
    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities[id] = f32v3(0.0f);
    }
    // TODO: I don't think we need to zero all of this, the modules should handle it
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
