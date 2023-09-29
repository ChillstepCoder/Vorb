#include "stdafx.h"
#include "CubemapRepository.h"

#include "serialization/YmlSerializer.h"
#include "rendering/texture/TextureHelpers.h"

#include <Vorb/io/IOManager.h>
#include "filesystem/FileSystem.h"

#include "io/PngLoader.h"
#include "util/TextureUtil.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"

// Match the shader TODO: profile 32?
constexpr GLuint WORK_GROUP_SIZE = 16;

struct LoadCubemapUserData {
    gli::texture2d mFacesRs[6];
};

struct CubemapFileData {
    nString mTexPosX;
    nString mTexNegX;
    nString mTexPosY;
    nString mTexNegY;
    nString mTexPosZ;
    nString mTexNegZ;
    vg::SamplerStateType mSamplerState = vg::SamplerStateType::LINEAR_CLAMP_MIPMAP;
};
SERIALIZABLE_SIMPLE(CubemapFileData,
    make_field(o.mTexPosX, "posx"sv),
    make_field(o.mTexNegX, "negx"sv),
    make_field(o.mTexPosY, "posy"sv),
    make_field(o.mTexNegY, "negy"sv),
    make_field(o.mTexPosZ, "posz"sv),
    make_field(o.mTexNegZ, "negz"sv),
    make_field(o.mSamplerState, "sampler_state"sv)
);

AssetLoadFunc CubemapRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        nString fileData = readFileToString(filePath);
        LoadCubemapUserData& loadData = std::any_cast<LoadCubemapUserData &>(userData);

        CubemapFileData cubeData;
        ryml::Tree tree = YmlSerializer::parseFileData(fileData);
        tree.crootref() >> cubeData;

        CubemapDef& cubemapDef = *static_cast<CubemapDef*>(assetDataPtr);

        const nString* facePaths[6] = {
            &cubeData.mTexPosX,
            &cubeData.mTexNegX,
            &cubeData.mTexPosY,
            &cubeData.mTexNegY,
            &cubeData.mTexPosZ,
            &cubeData.mTexNegZ
        };

        vio::Path directory = filePath;
        directory.trimEnd();

        for (int i = 0; i < 6; ++i) {

            const nString& str = *facePaths[i];
            if (!str.empty()) {

                // Get absolute path of texture.
                vio::Path resultPath;
                vio::Path texPath = directory / str;
                if (mIoManager.resolvePath(texPath, resultPath)) {
                    fs::path stdPath(resultPath.getString());
                    // Load the pixel data.
                    loadData.mFacesRs[i] = PngLoader::loadPng(stdPath, false /*flipV*/);
                    if (!loadData.mFacesRs[i].size()) {
                        panic("Empty cubemap texture {} for {}", str, filePath.getString());
                    }
                }
                else {
                    panic("Failed to find cubemap texture {} for {}", str, filePath.getString());
                }
            }
        }
        return true;
    };
}


AssetLoadFunc CubemapRepository::getAssetLoadRenderProcessFunc() {

    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        CubemapDef& cubemapDef = *static_cast<CubemapDef*>(assetDataPtr);
        LoadCubemapUserData& loadData = std::any_cast<LoadCubemapUserData&>(userData);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemapDef.mTexture);
        for (int i = 0; i < 6; ++i) {
            if (!initFace(cubemapDef, i, loadData.mFacesRs[i])) {
                panic("Failed to init cubemap face {} for {}", i, cubemapDef.getName().toString().c_str());
            }
        }

        // TODO: on demand? Cached?
        computePBRMaps(cubemapDef);
        return true;
    };
}


bool CubemapRepository::initFace(CubemapDef& def, int face, const gli::texture2d& rs) {
    const TextureUploadInfo uploadInfo = TextureHelpers::getTextureUploadInfo(rs);

    assert(rs.extent().x == rs.extent().y);
    assert(face >= 0 && face < 6);
    if (face == 0) {
        def.mDims.x = rs.extent().x;
        def.mDims.y = rs.extent().y;
        glTextureStorage2D(
            def.mTexture,
            1,           // one level, no mipmaps
            (VGEnum)uploadInfo.internalFormat,
            rs.extent().x,
            rs.extent().y
        );
    }
    else if (def.mDims.x != rs.extent().x || def.mDims.y != rs.extent().x) {
        panic("Cubemap texture size mismatch ({},{}) vs ({},{}). All faces must be the same size", rs.extent().x, rs.extent().y, def.mDims.x, def.mDims.y);
        return false;
    }

    glTextureSubImage3D(
        def.mTexture,
        0,
        0,
        0,
        face,
        rs.extent().x,
        rs.extent().y,
        1,      // depth how many faces to set, if this was 3 we'd set 3 cubemap faces at once
        (VGEnum)uploadInfo.textureFormat,
        (VGEnum)uploadInfo.texturePixelType,
        rs.data()
    );

    return true;
}


void CubemapRepository::computePBRMaps(CubemapDef& def) {
    createMipmaps(def); // Used for precomputed map
    computeIrradianceMap(def);
    computePrefilterMap(def);
}

void CubemapRepository::createMipmaps(CubemapDef& def) {
    // Create Mipmaps If Necessary
    ui32 mipmapLevels = computeMipmapCount(def.mDims, 16 /* arbitrary max*/);
    assert(mipmapLevels > 0);
    glTextureParameteri(def.mTexture, GL_TEXTURE_MAX_LOD, mipmapLevels);
    glTextureParameteri(def.mTexture, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
    glGenerateTextureMipmap(def.mTexture);
}

void CubemapRepository::computeIrradianceMap(CubemapDef& def)
{
    assert(def.mTexture);
    assert(!def.mIrradianceMap);
    assert(def.mDims.x == def.mDims.y);
    const int irradianceWidth = 64;
    // Allocate irradiance texture
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &def.mIrradianceMap);
    glTextureStorage2D(
        def.mIrradianceMap,
        1,           // one level, no mipmaps
        GL_RGBA16F,    // internal format
        irradianceWidth,
        irradianceWidth
    );
    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(def.mIrradianceMap);

    // Input
    glBindImageTexture(0, def.mTexture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
    // Output
    glBindImageTexture(1, def.mIrradianceMap, 0, false, 0, GL_WRITE_ONLY, GL_RGBA16F);
    
    // This should be in assets.preload
    const MaterialShaderDef& computeShader = MaterialShaderRepository::get().getLoadedAsset(CStrToken("cubemap_irradiance"));
    computeShader.useCompute();
    glUniform2f(computeShader.getUniform("unOutputDims"), (f32)irradianceWidth, (f32)irradianceWidth);
    glUniform2f(computeShader.getUniform("unInputDims"), (f32)def.mDims.x, (f32)def.mDims.y);
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

void CubemapRepository::computePrefilterMap(CubemapDef& def) {
    assert(!def.mPrefilterMap);
    const int precomputedWidth = 128;
    const int numMipMaps = computeMipmapCount(ui32v2(precomputedWidth), 10);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &def.mPrefilterMap);
    glTextureStorage2D(
        def.mPrefilterMap,
        numMipMaps,   // one level, no mipmaps
        GL_RGBA16F,    // internal format
        precomputedWidth,
        precomputedWidth
    );
    vg::sSamplerStates.LINEAR_CLAMP_MIPMAP.setForTexture(def.mPrefilterMap);
    assert(numMipMaps > 0);
    glTextureParameteri(def.mPrefilterMap, GL_TEXTURE_MAX_LOD, numMipMaps);
    glTextureParameteri(def.mPrefilterMap, GL_TEXTURE_MAX_LEVEL, numMipMaps);
    glGenerateTextureMipmap(def.mPrefilterMap);

    // This should be in assets.preload
    const MaterialShaderDef& computeShader = MaterialShaderRepository::get().getLoadedAsset(CStrToken("prefilter_ggx"));
    computeShader.useCompute();
    // Input
    glBindTextureUnit(0, def.mTexture);
    glUniform2f(computeShader.getUniform("unInputDims"), (f32)def.mDims.x, (f32)def.mDims.y);
    VGUniform unMipmapDims = computeShader.getUniform("unMipmapDims");
    VGUniform unRoughness = computeShader.getUniform("unRoughness");
    for (int mip = 0; mip < numMipMaps; ++mip) {
        const ui32 mipmapSize = precomputedWidth >> mip;
        const float roughness = (float)mip / (float)(numMipMaps - 1);
        glUniform2f(unMipmapDims, (f32)mipmapSize, (f32)mipmapSize);
        glUniform1f(unRoughness, roughness);

        // Output
        glBindImageTexture(1, def.mPrefilterMap, mip, false, 0, GL_WRITE_ONLY, GL_RGBA16F);

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

std::any CubemapRepository::getUserData(AssetID) {
    return LoadCubemapUserData();
}