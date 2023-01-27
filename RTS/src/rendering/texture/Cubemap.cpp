#include "stdafx.h"
#include "Cubemap.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "util/TextureUtil.h"

#include <Vorb/graphics/ImageIO.h>

// Match the shader TODO: profile 32?
constexpr GLuint WORK_GROUP_SIZE = 16;

KEG_TYPE_DEF(CubemapFileData, CubemapFileData, kt) {
    kt.addValue("posx", keg::Value::basic(offsetof(CubemapFileData, mTexPosX), keg::BasicType::STRING));
    kt.addValue("negx", keg::Value::basic(offsetof(CubemapFileData, mTexNegX), keg::BasicType::STRING));
    kt.addValue("posy", keg::Value::basic(offsetof(CubemapFileData, mTexPosY), keg::BasicType::STRING));
    kt.addValue("negy", keg::Value::basic(offsetof(CubemapFileData, mTexNegY), keg::BasicType::STRING));
    kt.addValue("posz", keg::Value::basic(offsetof(CubemapFileData, mTexPosZ), keg::BasicType::STRING));
    kt.addValue("negz", keg::Value::basic(offsetof(CubemapFileData, mTexNegZ), keg::BasicType::STRING));
    kt.addValue("sampler_state", keg::Value::custom(offsetof(CubemapFileData, mSamplerState), "SamplerStateType", true));
}

Cubemap::Cubemap(CubemapID id) : mId(id) {
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mTexture);
}

Cubemap::~Cubemap() {
    glDeleteTextures(1, &mTexture);
}

bool Cubemap::initFace(int face, const vg::ScopedBitmapResource& rs) {
    assert(rs.width == rs.height);
    assert(face >= 0 && face < 6);
    if (face == 0) {
        mDims.x = rs.width;
        mDims.y = rs.height;
        glTextureStorage2D(
            mTexture,
            1,           // one level, no mipmaps
            GL_RGBA8,    // internal format
            rs.width,
            rs.height
        );
    }
    else if (mDims.x != rs.width || mDims.y != rs.height) {
        LOG_CRITICAL("Cubemap texture size mismatch ({},{}) vs ({},{}). All faces must be the same size", rs.width, rs.height, mDims.x, mDims.y);
        return false;
    }

    glTextureSubImage3D(
        mTexture,
        0,
        0,
        0,
        face,
        rs.width,
        rs.height,
        1,      // depth how many faces to set, if this was 3 we'd set 3 cubemap faces at once
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        rs.data
    );

    return true;
}

void Cubemap::computePBRMaps() {
    createMipmaps(); // Used for precomputed map
    computeIrradianceMap();
    computePrecomputedMap();
}

void Cubemap::createMipmaps() {
    // Create Mipmaps If Necessary
    ui32 mipmapLevels = computeMipmapCount(mDims, 16 /* arbitrary max*/);
    assert(mipmapLevels > 0);
    glTextureParameteri(mTexture, GL_TEXTURE_MAX_LOD, mipmapLevels);
    glTextureParameteri(mTexture, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
    glGenerateTextureMipmap(mTexture);
}

void Cubemap::computeIrradianceMap() {
    assert(mTexture);
    assert(!mIrradianceMap);
    assert(mDims.x == mDims.y);
    const int irradianceWidth = 64;
    // Allocate irradiance texture
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mIrradianceMap);
    glTextureStorage2D(
        mIrradianceMap,
        1,           // one level, no mipmaps
        GL_RGBA16F,    // internal format
        irradianceWidth,
        irradianceWidth
    );
    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(mIrradianceMap);

    // Input
    glBindImageTexture(0, mTexture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
    // Output
    glBindImageTexture(1, mIrradianceMap, 0, false, 0, GL_WRITE_ONLY, GL_RGBA16F);

    const vg::GLProgram* computeShader = Services::ResourceManager::ref().getMaterialShaderManager().getComputeShader("cubemap_irradiance");
    computeShader->use();
    glUniform2f(computeShader->getUniform("unOutputDims"), (f32)irradianceWidth, (f32)irradianceWidth);
    glUniform2f(computeShader->getUniform("unInputDims"), (f32)mDims.x, (f32)mDims.y);
    if (irradianceWidth % WORK_GROUP_SIZE == 0) {
        const GLuint sz = (GLuint)irradianceWidth / WORK_GROUP_SIZE;
        glDispatchCompute(sz, sz, 6);
    }
    else {
        const GLuint sz = 1 + (GLuint)irradianceWidth / WORK_GROUP_SIZE;
        glDispatchCompute(sz, sz, 6);
    }
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Cubemap::computePrecomputedMap() {
    assert(!mPrecomputedMap);
    const int precomputedWidth = 128;
    const int numMipMaps = computeMipmapCount(ui32v2(precomputedWidth), 10);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mPrecomputedMap);
    glTextureStorage2D(
        mPrecomputedMap,
        numMipMaps,   // one level, no mipmaps
        GL_RGBA16F,    // internal format
        precomputedWidth,
        precomputedWidth
    );
    vg::sSamplerStates.LINEAR_CLAMP_MIPMAP.setForTexture(mPrecomputedMap);
    assert(numMipMaps > 0);
    glTextureParameteri(mPrecomputedMap, GL_TEXTURE_MAX_LOD, numMipMaps);
    glTextureParameteri(mPrecomputedMap, GL_TEXTURE_MAX_LEVEL, numMipMaps);
    glGenerateTextureMipmap(mPrecomputedMap);

    const vg::GLProgram* computeShader = Services::ResourceManager::ref().getMaterialShaderManager().getComputeShader("prefilter_ggx");
    computeShader->use();
    // Input
    glBindTextureUnit(0, mTexture);
    glUniform2f(computeShader->getUniform("unInputDims"), (f32)mDims.x, (f32)mDims.y);
    VGUniform unMipmapDims = computeShader->getUniform("unMipmapDims");
    VGUniform unRoughness = computeShader->getUniform("unRoughness");
    for (int mip = 0; mip < numMipMaps; ++mip) {
        const ui32 mipmapSize = precomputedWidth >> mip;
        const float roughness = (float)mip / (float)(numMipMaps - 1);
        glUniform2f(unMipmapDims, (f32)mipmapSize, (f32)mipmapSize);
        glUniform1f(unRoughness, roughness);

        // Output
        glBindImageTexture(1, mPrecomputedMap, mip, false, 0, GL_WRITE_ONLY, GL_RGBA16F);

        if (mipmapSize % WORK_GROUP_SIZE == 0) {
            const GLuint sz = (GLuint)mipmapSize / WORK_GROUP_SIZE;
            glDispatchCompute(sz, sz, 6);
        }
        else {
            const GLuint sz = 1 + (GLuint)mipmapSize / WORK_GROUP_SIZE;
            glDispatchCompute(sz, sz, 6);
        }
    }
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
