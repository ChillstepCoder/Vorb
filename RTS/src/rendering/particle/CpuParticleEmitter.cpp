#include "stdafx.h"
#include "CpuParticleEmitter.h"

#include "rendering/particle/CpuParticleEmitter.h"
#include "rendering/MaterialShader.h"

#include "definitions/ParticleSystemDef.h"

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
    allocateParticleData();

}

CpuParticleEmitter::CpuParticleEmitter(const ParticleEmitterDef& def) : mShader(*def.mShader) {

    mMaxParticles = def.mMaxParticles;
    mGlobalParticleScale = def.mDefaultScale;
    mGlobalParticleColor = def.mDefaultColor;
    mGlobalMaterialID = def.mDefaultMaterialID;
    mLifetimeSec = def.mLifetimeSec;
    mLooping = def.mLooping;

    const size_t totalModuleCount =
        def.mEmitterUpdateModules.size() +
        def.mParticleInitModules.size() +
        def.mParticleUpdateModules.size();
    mEmitterModuleMethods.reserve(totalModuleCount);

    for (auto&& module : def.mEmitterUpdateModules) {
        addEmitterUpdateModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    for (auto&& module : def.mParticleInitModules) {
        addParticleInitModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    for (auto&& module : def.mParticleUpdateModules) {
        addParticleUpdateModule(*module);
        mComponents |= module->getRequiredComponents();
    }

    allocateParticleData();
}

CpuParticleEmitter::~CpuParticleEmitter() {
}

bool CpuParticleEmitter::updateAndRender(f32 elapsedSec) {
    ASSERT_RENDER_THREAD();

    mLastElapsedSec = elapsedSec;
    mTotalElapsedSec += elapsedSec;

    if (mNativeUpdateFunction) {
        mDataChanged = true;
        mNativeUpdateFunction(*this, mParticleData, elapsedSec);
    }

    // Update emitter
    for (size_t i = 0; i < mNumEmitterUpdateMethods; ++i) {
        mEmitterModuleMethods[i](*this, INVALID_PARTICLE_ID, mParticleModuleData[i], elapsedSec);
    }

    // Run every particle through the modules
    // TODO: Multithreaded with triple buffer state?

#define UPDATE_LOGIC \
    if (mParticleData.mPositions[i].x == FLT_MAX) [[unlikely]] { \
        continue; \
    } \
    for (size_t j = mNumEmitterUpdateMethods + mNumParticleInitMethods; j < mEmitterModuleMethods.size(); ++j) { \
        mEmitterModuleMethods[j](*this, i, mParticleModuleData[j], elapsedSec); \
    }

    // Split out to avoid branching in critical path
    if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
        if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
            mDataChanged = true;

            for (ui32 i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
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
            for (ui32 i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
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

            for (ui32 i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
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
            for (ui32 i = mFirstActiveParticle; i <= mLastActiveParticle; ++i) {
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

void CpuParticleEmitter::addParticleVelocity(ParticleID id, f32v3 velocity) {
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
    mParticleData.mMaterials[id] = (ui32)material;
    mDataChanged = true;
}

f32 CpuParticleEmitter::getParticleNormalizedLifetime(ParticleID id) const {
    if (mParticleData.mLifespans) {
        return glm::min(mParticleData.mLifetimes[id] / mParticleData.mLifespans[id], 1.0f);
    }
    return glm::min(mParticleData.mLifetimes[id] / mGlobalParticleLifespan, 1.0f);
}

void CpuParticleEmitter::emitParticles(ui32v2 countRange) {
    
    ui32 emitCount = ((ui32)Random::getCachedRandom() % (countRange.y - countRange.x)) + countRange.x;
    emitCount = glm::min(emitCount, mActiveParticles - mMaxParticles);
    emitParticles(emitCount);
}

void CpuParticleEmitter::emitParticles(ui32 count) {
    if (count == 0) {
        return;
    }
    assert(mActiveParticles <= mMaxParticles);
    count = glm::min(count, mMaxParticles - mActiveParticles);
    mDataChanged = true;
    for (ui32 i = 0; i < count; ++i) {
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

void CpuParticleEmitter::allocateParticleData()
{
    ASSERT_RENDER_THREAD();
    assert(mMaxParticles <= MAX_PARTICLES);

    mParticleData.mPositions = std::make_unique<f32v3[]>(mMaxParticles);
    mParticleData.mLifetimes = std::make_unique<f32[]>(mMaxParticles);
    // These must be f32v4 to match std430 layout, so we always have rotation allocated on GPU even
    // if we arent using it (rotation is w component)
    mGpuData.mPositionsAndRotationsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v4));

    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities = std::make_unique<f32v3[]>(mMaxParticles);
        // No GPU data for velocities
        // TODO: VelocityCPU vs VelocityGPU
    }
    if (mComponents.isBitSet(ParticleComponentType::Scale)) {
        mParticleData.mScales = std::make_unique_for_overwrite<f32v2[]>(mMaxParticles);
        mGpuData.mScalesBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v2));
        std::fill_n(mParticleData.mScales.get(), mMaxParticles, f32v2(1.0f)); // Default values
    }
    if (mComponents.isBitSet(ParticleComponentType::HDRColor)) {
        mParticleData.mHDRColors = std::make_unique<f32v4[]>(mMaxParticles);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(f32v4));
    }
    else if (mComponents.isBitSet(ParticleComponentType::Color)) {
        mParticleData.mColors = std::make_unique<color4[]>(mMaxParticles);
        mGpuData.mColorsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(color4));
    }
    if (mComponents.isBitSet(ParticleComponentType::Lifespan)) {
        mParticleData.mLifespans = std::make_unique<f32[]>(mMaxParticles);
        // No GPU data for lifespans
    }
    if (mComponents.isBitSet(ParticleComponentType::MaterialID)) {
        mParticleData.mMaterials = std::make_unique<ui32[]>(mMaxParticles);
        mGpuData.mMaterialsBuffer = std::make_unique<GpuStreamingDataBuffer>(mMaxParticles, sizeof(ui32));
    }
    if (mComponents.isBitSet(ParticleComponentType::Rotation)) {
        mParticleData.mRotations = std::make_unique<f32[]>(mMaxParticles);
        std::memset(mParticleData.mRotations.get(), 0, mMaxParticles * sizeof(f32)); // Default values
        // Rotations are packed into mPositionsAndRotationsBuffer which is always allocated
    }

    static_assert(e_cast(ParticleComponentType::TERM) == 65);
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
