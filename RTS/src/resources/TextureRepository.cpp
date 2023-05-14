#include "stdafx.h"
#include "TextureRepository.h"

#include "rendering/texture/MaterialTextureGenerator.h"

#include "util/TextureUtil.h"

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/ImageIO.h> // TODO: REMOVE

#include "io/PngLoader.h"

#include "filesystem/FileSystem.h"

#include <gli/gli.hpp>

// TODO: https://github.com/nothings/stb

TextureRepository::TextureRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {
    mNormalMapGenerator = std::make_unique<MaterialTextureGenerator>();
    mNormalMapGenerator->init();
}

TextureRepository::~TextureRepository() {

}

const TextureData* TextureRepository::loadTexture(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, vg::TextureInternalFormat internalFormat, bool flipV, gli::texture2d* outRs/* = nullptr*/) {
    // TODO: Test using temporary nString buffer memory so we dont keep heap allocating all these strings
    const nString textureName = vio::getLeafNameFromFilePathNoExtension(filePath);


    // Check if the texture is already cached.
    /*Texture texture = findTexture(textureName);
    if (texture.id) return texture;*/
    // Allow caller to optionally hold data
    gli::texture2d rs;
    gli::texture2d* rsPtr = &rs;
    if (outRs) {
        rsPtr = outRs;
    }

    // Get absolute path of texture.
    vio::Path texPath;
    mIoManager.resolvePath(filePath, texPath);

    fs::path stdPath(texPath.getString());
    const nString extension = stdPath.extension().string();

    const time_t fileLastWriteTime = FileSystem::getLastFileWriteTime(stdPath);

    // NOTES
    // 1. Load with lodepng-turbo
    // 2. Compress at run time with richgel999/bc7enc
    // BC1 = DXT1 = RGB
    // BC3 = DXT5 = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    // BC4 = Grayscale
    // BC5 = RG = Tangent space normal maps

    // .dds files will be created from PNG on the fly and cached to make future loading faster
    if (extension == ".png") {
        bool needsGenerateDDS = true;
        // Check if there is a .dds already
        fs::path ddsPath = stdPath;
        ddsPath.replace_extension(".dds");

        if (fs::is_regular_file(ddsPath)) {
            if (FileSystem::getLastFileWriteTime(ddsPath) >= fileLastWriteTime) {
                needsGenerateDDS = false;
            }
        }

        if (needsGenerateDDS) {
            // Load PNG
           // if (!loadPngDataInternal(texPath, flipV, rsPtr)) {
           //      return nullptr;
           // }
            *rsPtr = PngLoader::loadPng(stdPath, flipV);

            // Save DDS file
            // Uncompressed gli texture
           // assert(rsPtr->format() == gli::FORMAT_RGBA8_UNORM_PACK8);

            // Compress to DXT5
            //gli::texture2d textureDXT5 = gli::convert(uncompressedTexture, gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16);
            
            //if (!gli::save(textureDXT5, ddsPath.string())) {
            //    LOG_CRITICAL("Failed to save DDS {}", ddsPath.string());
            //}
            
        }
        else {
            // Load DDS directly
            assert(false);
        }
    }
    else {
        assert(false);
    }

    GLTexture texture = uploadTexture(*rsPtr, type, *samplerState, INT_MAX);
    assert(texture.isValid());

    TextureData* textureData;
    TextureID textureId;
    auto&& it = mTextureIdLookup.find(textureName);
    if (it != mTextureIdLookup.end()) {
        // Replace existing
        LOG_CRITICAL("Replacing existing texture {}", filePath.getString());
        assert(false); // No reason for this right now.
        textureId = it->second;
        textureData = &mTextures[textureId];
        textureData->texture.destroy();
    }
    else {
        textureId = mTextures.size();
        textureData = &mTextures.emplace_back();
        mTextureIdLookup[textureName] = textureId;
    }

    textureData->texture = std::move(texture);
    textureData->type = type;
    textureData->textureId = textureId;
    textureData->texturePath = filePath;
    textureData->samplerState = samplerState;
    textureData->flipV = flipV;
    return textureData;
    // TODO: dirty buffer bit?
}

const TextureData& TextureRepository::getTexture(const nString& textureName) const {
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
                vg::ScopedBitmapResource rs(vg::ImageIO().loadPng(resultPath.getString(), vg::ImageIOFormat::RGBA_UI8, true /*flipv*/));
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

bool TextureRepository::loadRawPngData(const vio::Path& filePath, OUT vg::ScopedBitmapResource& outRs, bool flipV) {
    // Get absolute path of texture.
    vio::Path texPath;
    mIoManager.resolvePath(filePath, texPath);

    // Load the pixel data.
    return loadPngDataInternal(texPath, flipV, &outRs);
}

bool TextureRepository::loadPngDataInternal(const vio::Path& filePath, bool flipV, vg::ScopedBitmapResource* outRs) {
    *outRs = vg::ImageIO().loadPng(filePath.getString(), vg::ImageIOFormat::RGBA_UI8, !flipV /*inverted on purpose*/);
    if (outRs->data == nullptr) {
        LOG_CRITICAL("Failed to load PNG data for {}", filePath.getString());
        return false;
    }
    return true;
}

GLTexture TextureRepository::uploadTexture(const void* data, ui32v2 dims, vg::TexturePixelType texturePixelType, vg::TextureTarget textureTarget, const vg::SamplerState* samplingParameters, vg::TextureInternalFormat internalFormat, vg::TextureFormat textureFormat, i32 mipmapLevels) {
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    mipmapLevels = computeMipmapCount(dims, mipmapLevels);

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

GLTexture TextureRepository::uploadTexture(const gli::texture2d& textureData, vg::TextureTarget textureTarget, const vg::SamplerState& samplerState, i32 maxMipLevels) {
    assert(!textureData.empty());
    const ui32v2 dims(textureData.extent().x, textureData.extent().y);
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    int mipmapLevels = computeMipmapCount(dims, maxMipLevels);
    // TODO: Currently mipmapLevels is -1...
    //assert(mipmapLevels == textureData.levels()); 

    vg::TextureInternalFormat internalFormat;
    vg::TexturePixelType texturePixelType;
    vg::TextureFormat textureFormat;
    switch (textureData.format()) {
        case gli::FORMAT_R8_UNORM_PACK8:
            internalFormat = vg::TextureInternalFormat::R8;
            texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
            textureFormat = vg::TextureFormat::RED;
            break;
        case gli::FORMAT_RG8_UNORM_PACK8:
            internalFormat = vg::TextureInternalFormat::RG8;
            texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
            textureFormat = vg::TextureFormat::RG;
            break;
        case gli::FORMAT_RGB8_UNORM_PACK8:
            internalFormat = vg::TextureInternalFormat::RGB8;
            texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
            textureFormat = vg::TextureFormat::RGB;
            break;
        case gli::FORMAT_RGBA8_UNORM_PACK8:
            internalFormat = vg::TextureInternalFormat::RGBA8;
            texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
            textureFormat = vg::TextureFormat::RGBA;
            break;
        default:
            assert(false);
    }

    switch (textureTarget) {
        case vg::TextureTarget::TEXTURE_1D:
        case vg::TextureTarget::PROXY_TEXTURE_1D:
            glTextureStorage1D(handle, mipmapLevels, (VGEnum)internalFormat, dims.x);
            glTextureSubImage1D(handle, 0, 0, dims.x, (VGEnum)textureFormat, (VGEnum)texturePixelType, textureData.data());
            break;
        case vg::TextureTarget::TEXTURE_2D:
            glTextureStorage2D(handle, mipmapLevels, (VGEnum)internalFormat, dims.x, dims.y);
            glTextureSubImage2D(handle, 0, 0, 0, dims.x, dims.y, (VGEnum)textureFormat, (VGEnum)texturePixelType, textureData.data());
            break;
        default:
            assert(false);
            break;
    }
    checkGlError("TextureRepository::uploadTexture");
    // Setup Texture Sampling Parameters
    samplerState.setForTexture(handle);

    // Create Mipmaps If Necessary
    if (mipmapLevels > 0) {
        glTextureParameteri(handle, GL_TEXTURE_MAX_LOD, mipmapLevels);
        glTextureParameteri(handle, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
        glGenerateTextureMipmap(handle);
    }

    return GLTexture(handle, textureTarget, dims);
}
