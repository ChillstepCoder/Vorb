#include "stdafx.h"
#include "AmbientOcclusionPostProcess.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"

#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

#include <random>

#include <Vorb/graphics/GBuffer.h>

// Match shader
const int KERNEL_SIZE = 32;

AmbientOcclusionPostProcess::AmbientOcclusionPostProcess(const ui32v2& gbufferDims) {
    // Color
    // TODO: SWAP CHAIN
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::R8);
    }

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mMaterial = materialManager.getMaterialShader("ssao");
    mApplyMaterial = materialManager.getMaterialShader("ssao_apply");
    mBlurMaterial = materialManager.getMaterialShader("gaussian_blur_r");

    //https://learnopengl.com/Advanced-Lighting/SSAO
    // Build kernel
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    mSsaoKernel.reserve(KERNEL_SIZE);
    for (unsigned int i = 0; i < KERNEL_SIZE; ++i)
    {
        f32v3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = (float)i / 64.0;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        mSsaoKernel.push_back(sample);
    }

    // Build noise
    std::vector<glm::vec3> ssaoNoise;
    ssaoNoise.reserve(16);
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    glGenTextures(1, &mNoiseTexture);
    glBindTexture(GL_TEXTURE_2D, mNoiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    checkGlError("init DepthOfFieldPostProcess");
}

void AmbientOcclusionPostProcess::render(vg::GBuffer* activeGBuffer)
{
    if (sDebugOptions.mSSAODisabled) {
        mGBuffers[0]->use();
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        activeGBuffer->use();
        return;
    }

    mGBuffers[0]->use();

    ui32 nextTexture;
    MaterialRenderer::bindMaterialForRender(*mMaterial, &nextTexture);
    vg::BlendState& PREV_BLEND = vg::BlendState::PREV;
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::REPLACE);

    const VGUniform& noiseUniform = mMaterial->mProgram.getUniform("unTexNoise");
    const VGUniform& samplesUniform = mMaterial->mProgram.getUniform("unSamples[0]");
    const VGUniform& biasUniform = mMaterial->mProgram.getUniform("unBias");
    const VGUniform& radiusUniform = mMaterial->mProgram.getUniform("unRadius");
    const VGUniform& rangeUniform = mMaterial->mProgram.getUniform("unRangeCheckMult");

    glUniform1f(biasUniform, sDebugOptions.mSSAOBias);
    glUniform1f(radiusUniform, sDebugOptions.mSSAORadius);
    glUniform1f(rangeUniform, sDebugOptions.mSSAORangeCheckMult);

    // Send noise texture
    glActiveTexture(GL_TEXTURE0 + nextTexture);
    glUniform1i(noiseUniform, nextTexture);
    glBindTexture(GL_TEXTURE_2D, mNoiseTexture);

    // Send samples
    glUniform3fv(samplesUniform, KERNEL_SIZE, &mSsaoKernel[0].x);

    sGlobalFullQuadVBO.draw();

    /*glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    mGBuffers[0].use();
    glClear(GL_COLOR_BUFFER_BIT);
    mGBuffers[1].use();
    glClear(GL_COLOR_BUFFER_BIT);*/

    // Blur it
    MaterialRenderer::bindMaterialForRender(*mBlurMaterial, &nextTexture);
    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    mGBuffers[0]->bindAlbedoTexture(nextTexture);
    for (int i = 0; i < sDebugOptions.mSSAOBlurPasses; ++i) {

        // Horizontal
        mGBuffers[1]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mSSAOBlurRadius, 0.0f);
        sGlobalFullQuadVBO.draw();

        // Vertical
        mGBuffers[1]->bindAlbedoTexture(nextTexture);
        mGBuffers[0]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mSSAOBlurRadius);
        sGlobalFullQuadVBO.draw();

        mGBuffers[0]->bindAlbedoTexture(nextTexture);
    }

    vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);

    // Apply to gbuffer
    activeGBuffer->use();
    MaterialRenderer::bindMaterialForRender(*mApplyMaterial, &nextTexture);
    sGlobalFullQuadVBO.draw();

    vg::DepthState::restorePrevious();
    PREV_BLEND.set();

    checkGlError("Ambient Occlusion");
}

VGTexture AmbientOcclusionPostProcess::getSSAOTexture() const {
    return mGBuffers[0]->getAlbedoTexture();
}
