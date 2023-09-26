#include "stdafx.h"
#include "BrdfLUT.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"

#include <Vorb/graphics/SamplerState.h>

VGTexture BrdfLUT::sTexture = 0;

const ui32 TEXTURE_SIZE = 512;
const ui32 WORK_GROUP_SIZE = 16;
static_assert(TEXTURE_SIZE % WORK_GROUP_SIZE == 0);

void BrdfLUT::loadOrComputeTexture() {
    assert(!sTexture);
    // TODO: load cached

    glCreateTextures(GL_TEXTURE_2D, 1, &sTexture);
    glTextureStorage2D(
        sTexture,
        1,           // one level, no mipmaps
        GL_RG16F,    // internal format
        TEXTURE_SIZE,
        TEXTURE_SIZE
    );
    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(sTexture);

    // Input
    glBindImageTexture(0, sTexture, 0, false, 0, GL_WRITE_ONLY, GL_RG16F);

    // This is preloaded
    const MaterialShaderDef* computeShader = MaterialShaderRepository::get().tryGetLoadedAsset(StrToken("integrate_brdf", 0));
    if (!computeShader) panic("integrate_brdf shader was not preloaded. Make sure it is in .preload");
    computeShader->useCompute();

    const GLuint sz = (GLuint)TEXTURE_SIZE / WORK_GROUP_SIZE;
    glDispatchCompute(sz, sz, 1);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
