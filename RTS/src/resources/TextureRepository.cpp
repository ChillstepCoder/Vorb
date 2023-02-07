#include "stdafx.h"
#include "TextureRepository.h"

#include "rendering/texture/MaterialTextureGenerator.h"

#include "util/TextureUtil.h"

#include <Vorb/graphics/TextureCache.h>

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

TextureRepository::TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager) : mTextureCache(textureCache), mIoManager(ioManager) {
    mNormalMapGenerator = std::make_unique<MaterialTextureGenerator>();
    mNormalMapGenerator->init();
}

TextureRepository::~TextureRepository() {

}

const TextureData* TextureRepository::loadTextureNew(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, vg::TextureInternalFormat internalFormat, bool flipV) {
    // TODO: Test using temporary nString buffer memory so we dont keep heap allocating all these strings
    nString textureName = vio::getLeafNameFromFilePathNoExtension(filePath);

    // Check if the texture is already cached.
    /*Texture texture = findTexture(textureName);
    if (texture.id) return texture;*/
    GLTexture texture;

    switch (type)
    {
        case vg::TextureTarget::TEXTURE_2D:
        {

            // Get absolute path of texture.
            vio::Path texPath;
            mIoManager.resolvePath(filePath, texPath);

            // Load the pixel data.
            vg::ScopedBitmapResource rs(vg::ImageIO().load(texPath.getString(), vg::ImageIOFormat::RGBA_UI8, !flipV /*inverted on purpose*/));
            if (!rs.data) return nullptr;

            texture = uploadTexture(rs.bytesUI8,
                ui32v2(rs.width, rs.height),
                vg::TexturePixelType::UNSIGNED_BYTE,
                type,
                samplerState,
                internalFormat,
                vg::TextureFormat::RGBA,
                INT_MAX /*mipmap levels*/);
            break;
        }
        default:
            assert(false && "Only TEXTURE_2D is supported currently");
    }

    assert(texture.isValid());

    TextureData* textureData;
    TextureID textureId;
    auto&& it = mTextureIdLookup.find(textureName);
    if (it != mTextureIdLookup.end()) {
        // Replace existing
        LOG_WARN("Replacing existing texture {}", filePath.getString());
        textureId = it->second;
        textureData = &mTextures[textureId];
        textureData->texture.destroy();
    }
    else {
        textureId = mTextures.size();
        textureData = &mTextures.emplace_back();
        mTextureIdLookup[textureName] = textureId;
    }
    // Track relative to path as well in case we care
    mTextureAssetPaths[filePath.getString()] = textureId;

    textureData->texture = std::move(texture);
    textureData->type = type;
    textureData->textureId = textureId;
    textureData->texturePath = filePath;
    textureData->samplerState = samplerState;
    textureData->flipV = flipV;
    return textureData;
    // TODO: dirty buffer bit?
}

const TextureData& TextureRepository::getTextureNew(const nString& textureName) const {
    auto&& it = mTextureIdLookup.find(textureName);
    if (it == mTextureIdLookup.end()) {
        LOG_CRITICAL("Failed to find texture {} make sure there is a .material for it", textureName);
        assert(false);
    }
    return mTextures[it->second];
}

const Cubemap* TextureRepository::loadCubemap(const vio::Path& cubeFilePath) {
    CubemapFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, cubeFilePath, &KEG_GLOBAL_TYPE(CubemapFileData), false /*allowEmpty*/)) {
        LOG_CRITICAL("Failed to parse cubemap {}", cubeFilePath.getString());
        return nullptr;
    }
    const nString* facePaths[6] = {
        &fileData.mTexPosX,
        &fileData.mTexNegX,
        &fileData.mTexPosY,
        &fileData.mTexNegY,
        &fileData.mTexPosZ,
        &fileData.mTexNegZ
    };

    vio::Path directory = cubeFilePath;
    directory.trimEnd();

    CubemapID id = mCubemaps.size();
    Cubemap& cubemap = *mCubemaps.emplace_back(std::make_unique<Cubemap>(id));
    mCubemapIdLookup[cubeFilePath.getFileNameNoExtension()] = id;

    VGTexture texture = cubemap.getTexture();

    for (int i = 0; i < 6; ++i) {

        const nString& str = *facePaths[i];
        if (str.size()) {

            // Get absolute path of texture.
            vio::Path resultPath;
            vio::Path texPath = directory / str;
            if (mIoManager.resolvePath(texPath, resultPath)) {

                // Load the pixel data.
                vg::ScopedBitmapResource rs(vg::ImageIO().load(resultPath.getString(), vg::ImageIOFormat::RGBA_UI8, true /*flipv*/));
                if (!rs.data) {
                    LOG_CRITICAL("Empty cubemap texture {} for {}", str, cubeFilePath.getString());
                    return nullptr;
                }

                if (!cubemap.initFace(i, rs)) {
                    LOG_CRITICAL("Failed to init cubemap face {} for {}", str, cubeFilePath.getString());
                }
            }
            else {
                LOG_CRITICAL("Failed to find cubemap texture {} for {}", str, cubeFilePath.getString());
                return nullptr;
            }
        }
    }

    // TODO: on demand? Cached?
    cubemap.computePBRMaps();

    return &cubemap;
}

const Cubemap& TextureRepository::getCubemap(const nString& cubemapName) const {
    auto&& it = mCubemapIdLookup.find(cubemapName);
    if (it == mCubemapIdLookup.end()) {
        LOG_CRITICAL("Failed to find cubemap {} make sure there is a .cube for it", cubemapName);
        assert(false);
    }
    return *mCubemaps[it->second];
}

const Cubemap& TextureRepository::getCubemap(CubemapID cubemapId) const {
    return *mCubemaps[cubemapId];
}

void TextureRepository::setTextureAssetPaths(const std::vector<vio::Path>& paths) {
    mTextureAssetPaths.clear();
    for (auto& path : paths) {
        mTextureAssetPaths[path.getString()] = INVALID_TEXTURE_ID;
    }
}

bool TextureRepository::loadRawTextureData(const vio::Path& filePath, OUT vg::ScopedBitmapResource& outRs, bool flipV) {
    // Get absolute path of texture.
    vio::Path texPath;
    mIoManager.resolvePath(filePath, texPath);

    // Load the pixel data.
    outRs = vg::ImageIO().load(texPath.getString(), vg::ImageIOFormat::RGBA_UI8, !flipV /*inverted on purpose*/);
    return outRs.data != nullptr;
}

GLTexture TextureRepository::uploadTexture(const void* data, ui32v2 dims, vg::TexturePixelType texturePixelType, vg::TextureTarget textureTarget, const vg::SamplerState* samplingParameters, vg::TextureInternalFormat internalFormat, vg::TextureFormat textureFormat, i32 mipmapLevels) {
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    mipmapLevels = computeMipmapCount(dims, mipmapLevels);


    // "Bind" the newly created texture : all future texture functions will modify this texture
    switch (textureTarget) {
        case vg::TextureTarget::TEXTURE_1D:
        case vg::TextureTarget::PROXY_TEXTURE_1D:
            glTextureStorage1D(handle, mipmapLevels, (VGEnum)internalFormat, dims.x);
            glTextureSubImage1D(handle, 0, 0, dims.x, (VGEnum)textureFormat, (VGEnum)texturePixelType, data);
            break;
        case vg::TextureTarget::TEXTURE_2D:
            glTextureStorage2D(handle, mipmapLevels, (VGEnum)internalFormat, dims.x, dims.y);
            glTextureSubImage2D(handle, 0, 0, 0, dims.x, dims.y, (VGEnum)textureFormat, (VGEnum)texturePixelType, data);
            break;
        default:
            assert(false);
            break;
    }
    checkGlError("TextureRepository::uploadTexture");
    // Setup Texture Sampling Parameters
    assert(samplingParameters);
    samplingParameters->setForTexture(handle);

    // Create Mipmaps If Necessary
    if (mipmapLevels > 0) {
        glTextureParameteri(handle, GL_TEXTURE_MAX_LOD, mipmapLevels);
        glTextureParameteri(handle, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
        glGenerateTextureMipmap(handle);
    }

    return GLTexture(handle, textureTarget, dims);
}
