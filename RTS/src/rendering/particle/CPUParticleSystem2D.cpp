#include "stdafx.h"
#include "CPUParticleSystem2D.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>

// Arbitrary for estimated perf reasons
constexpr ui32 MAX_PARTICLES = 20000;

CPUParticleSystem2D::CPUParticleSystem2D(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components) :
    mUpdateFunction(updateFunction),
    mMaxParticles(maxParticles),
    mComponents(components)
{
    ASSERT_RENDER_THREAD();
    assert(mMaxParticles <= MAX_PARTICLES);

    mParticleData.mPositions = std::unique_ptr<f32v2[]>(new f32v2[mMaxParticles]);
    mGpuData.mPositionsBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(f32v2));

    if (mComponents.isBitSet(ParticleComponentType::Velocity)) {
        mParticleData.mVelocities = std::unique_ptr<f32v2[]>(new f32v2[mMaxParticles]);
        // No GPU data for velocities
    }
    if (mComponents.isBitSet(ParticleComponentType::Scale)) {
        mParticleData.mScales = std::unique_ptr<f32v2[]>(new f32v2[mMaxParticles]);
        mGpuData.mScalesBuffer = std::make_unique<GpuStreamingDataBuffer>(maxParticles, sizeof(f32v2));
    }
    if (mComponents.isBitSet(ParticleComponentType::Color)) {
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

    static_assert(e_cast(ParticleComponentType::TERM) == 17);
}

void CPUParticleSystem2D::updateAndRender(VGUniform particleIndexUniform, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();

    mTotalElapsedSec = elapsedSec;

    // If we dont have an update function, we are a static system or manually updated system
    // which only needs to update GPU data on particle add or remove or manual flag dirty
    if (mUpdateFunction) {
        mDataChanged = true;
        mUpdateFunction(*this, mParticleData, elapsedSec);
    }

    render(particleIndexUniform);
}

ParticleID CPUParticleSystem2D::tryAddParticle(f32v2 position) {
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
    onNewParticleAdded(mActiveParticles);
    return mActiveParticles - 1;
}

void CPUParticleSystem2D::removeParticle(ParticleID id) {
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
    mParticleData.mPositions[id] = f32v2(FLT_MAX);
    mFreeParticleIDs.emplace_back(id);
}

void CPUParticleSystem2D::setParticlePosition(ParticleID id, f32v2 position) {
    mParticleData.mPositions[id] = position;
    mDataChanged = true;
}

void CPUParticleSystem2D::setParticleScale(ParticleID id, f32v2 scale) {
    mParticleData.mScales[id] = scale;
    mDataChanged = true;
}

void CPUParticleSystem2D::setParticleVelocity(ParticleID id, f32v2 velocity) {
    mParticleData.mVelocities[id] = velocity;
    mDataChanged = true;
}

void CPUParticleSystem2D::setParticleColor(ParticleID id, color4 color) {
    mParticleData.mColors[id] = color;
    mDataChanged = true;
}

void CPUParticleSystem2D::setParticleMaterial(ParticleID id, MaterialID material) {
    mParticleData.mMaterials[id] = (ui32)material;
    mDataChanged = true;
}

void CPUParticleSystem2D::render(VGUniform bufferIndexUniform) {

    if (mActiveParticles == 0) {
        return;
    }

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
    mGpuData.mPositionsBuffer->bindBufferAsSSBO(BUFFER_BASE_POSITIONS_SSBO);

    if (mDataChanged) {
        mDataChanged = false;
        // Positions
        f32v2* positions = (f32v2*)mGpuData.mPositionsBuffer->frameBeginAndGetDataForUpdate();
        memcpy(positions, &mParticleData.mPositions[mFirstActiveParticle].x, sizeof(f32v2) * particlesToRender);
        mCurrentUniformBufferStartIndex = mGpuData.mPositionsBuffer->flushDataAndIncrementFrame(particlesToRender);

        // Scales
        if (mGpuData.mScalesBuffer) {
            f32v2* scales = (f32v2*)mGpuData.mScalesBuffer->frameBeginAndGetDataForUpdate();
            memcpy(scales, &mParticleData.mScales[mFirstActiveParticle].x, sizeof(f32v2) * particlesToRender);
            assert(mCurrentUniformBufferStartIndex == mGpuData.mScalesBuffer->flushDataAndIncrementFrame(particlesToRender));
            mGpuData.mScalesBuffer->bindBufferAsSSBO(BUFFER_BASE_SCALES_SSBO);
        }

        // Colors
        if (mGpuData.mColorsBuffer) {
            color4* colors = (color4*)mGpuData.mColorsBuffer->frameBeginAndGetDataForUpdate();
            memcpy(colors, &mParticleData.mPositions[mFirstActiveParticle].x, sizeof(color4) * particlesToRender);
            assert(mCurrentUniformBufferStartIndex == mGpuData.mColorsBuffer->flushDataAndIncrementFrame(particlesToRender));
            mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_COLORS_SSBO);
        }

        // Materials
        if (mGpuData.mMaterialsBuffer) {
            ui32* materials = (ui32*)mGpuData.mMaterialsBuffer->frameBeginAndGetDataForUpdate();
            memcpy(materials, &mParticleData.mMaterials[mFirstActiveParticle], sizeof(ui32) * particlesToRender);
            assert(mCurrentUniformBufferStartIndex == mGpuData.mMaterialsBuffer->flushDataAndIncrementFrame(particlesToRender));
            mGpuData.mMaterialsBuffer->bindBufferAsSSBO(BUFFER_BASE_MATERIALS_SSBO);
        }
    }
    else {
        // Scales
        if (mGpuData.mScalesBuffer) {
            mGpuData.mScalesBuffer->bindBufferAsSSBO(BUFFER_BASE_SCALES_SSBO);
        }

        // Colors
        if (mGpuData.mColorsBuffer) {
            mGpuData.mColorsBuffer->bindBufferAsSSBO(BUFFER_BASE_COLORS_SSBO);
        }
        
        // Materials
        if (mGpuData.mMaterialsBuffer) {
            mGpuData.mMaterialsBuffer->bindBufferAsSSBO(BUFFER_BASE_MATERIALS_SSBO);
        }
    }
    static_assert(e_cast(ParticleComponentType::TERM) == 17);
    
    // Tell shader where our data is starting
    glUniform1i(bufferIndexUniform, mCurrentUniformBufferStartIndex);

    // Render two triangles per particle with no vertex data
    sGlobalFullTriangleVAO.drawNTriangles(particlesToRender * 2);
}

void CPUParticleSystem2D::onNewParticleAdded(ParticleID id) {
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
