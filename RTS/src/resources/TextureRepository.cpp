#include "stdafx.h"
#include "TextureRepository.h"

#include "rendering/texture/TextureConvert.h"
#include "rendering/texture/TextureHelpers.h"
#include "rendering/texture/MaterialTextureGenerator.h"

#include "resources/ResourceManager.h"

#include "util/TextureUtil.h"

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

#include "io/PngLoader.h"

#include "filesystem/FileSystem.h"

#include <gli/gli.hpp>
#include <gli/texture.hpp>

TextureRepository::TextureRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {
    mNormalMapGenerator = std::make_unique<MaterialTextureGenerator>();
    mNormalMapGenerator->init();
    TextureConvert::initConverters();
}

TextureRepository::~TextureRepository() {

}

const TextureData* TextureRepository::loadTexture(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, bool flipV, gli::texture2d* outRs/* = nullptr*/) {
    // TODO: Test using temporary nString buffer memory so we dont keep heap allocating all these strings
    const nString textureName = vio::getLeafNameFromFilePathNoExtension(filePath);
    LOG_INFO("Loading texture {}", filePath.getString());

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

    const fs::path resourceRoot(Services::ResourceManager::ref().getResourceRoot().getString());

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
    GLTexture texture;
    // .dds files will be created from PNG on the fly and cached to make future loading faster
    if (extension == ".png") {
        bool needsGenerateDDS = true;
        // Check if there is a .dds already
        // TODO: GET CACHE PATH TARGET
        fs::path ddsPath = stdPath;
        ddsPath.replace_extension(".dds"); // TODO: DETECT DDS CORRUPTION

        // Get relative to resource root
        ddsPath = ddsPath.lexically_relative(resourceRoot);
        ddsPath = resourceRoot / "_cache" / ddsPath;

        if (fs::is_regular_file(ddsPath)) {
            if (FileSystem::getLastFileWriteTime(ddsPath) >= fileLastWriteTime) {
                needsGenerateDDS = false;
            }
        }

        if (needsGenerateDDS || outRs) {
            *rsPtr = PngLoader::loadPng(stdPath, flipV);

            if (outRs) {
                // If caller requires full data, we wont ever generate dds
                texture = uploadTexture(*rsPtr, type, *samplerState, INT_MAX);
            } else {
                // Compression
                gli::texture2d ddsTexture = TextureConvert::convertToDDS(*rsPtr);
                texture = uploadDDSTexture(ddsTexture, type, *samplerState, INT_MAX);

                // Cache to disk
                LOG_TRACE("  Saving to disk - {}", ddsPath.string());

                // Ensure directories exist
                fs::path directoryPath = ddsPath;
                directoryPath._Remove_filename_and_separator();
                if (!std::filesystem::exists(directoryPath) && !std::filesystem::create_directories(directoryPath)) {
                    LOG_CRITICAL("  Failed to create directories for {}", directoryPath.string());
                }

                if (gli::save(ddsTexture, ddsPath.string())) {
                    LOG_TRACE("  Done");
                }
                else {
                    LOG_CRITICAL("  FAILED! Ensure directory exists or ensure program has permission to create folders");
                }
            }
            
        }
        else {
            // Assume 2d texture (potentially unsafe?)
            LOG_INFO("  Loading cached dds");
            gli::texture2d ddsTexture(gli::load(ddsPath.string()));
            texture = uploadDDSTexture(ddsTexture, type, *samplerState, INT_MAX);
        }
    }
    else {
        assert(false);
    }

    //texture = uploadTexture(*rsPtr, type, *samplerState, INT_MAX);
    assert(texture.isValid());

    TextureData* textureData;
    TextureID textureId;
    auto&& it = mTextureIdLookup.find(textureName);
    if (it != mTextureIdLookup.end()) {
        // Replace existing
        LOG_CRITICAL("  Replacing existing texture {}", filePath.getString());
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
        if (!str.empty()) {

            // Get absolute path of texture.
            vio::Path resultPath;
            vio::Path texPath = directory / str;
            if (mIoManager.resolvePath(texPath, resultPath)) {
                fs::path stdPath(resultPath.getString());
                // Load the pixel data.
                gli::texture2d texture = PngLoader::loadPng(stdPath, false /*flipV*/);
                if (!texture.size()) {
                    LOG_CRITICAL("Empty cubemap texture {} for {}", str, cubeFilePath.getString());
                    return nullptr;
                }

                if (!cubemap.initFace(i, texture)) {
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

gli::texture2d TextureRepository::loadRawPngData(const vio::Path& filePath, bool flipV) {

    // Get absolute path of texture.
    vio::Path resultPath;
    mIoManager.resolvePath(filePath, resultPath);

    // Load the pixel data.
    return PngLoader::loadPng(fs::path(resultPath.getString()), flipV);
}


GLTexture TextureRepository::uploadTexture(const gli::texture2d& textureData, vg::TextureTarget textureTarget, const vg::SamplerState& samplerState, i32 maxMipLevels) {
    assert(!textureData.empty());
    const ui32v2 dims(textureData.extent().x, textureData.extent().y);
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    //const i32 mipmapLevels = glm::min(maxMipLevels, (i32)textureData.levels());
    // TODO: GLI IS 1 LESS???
    const i32 mipmapLevels = computeMipmapCount(dims, maxMipLevels);

    const TextureUploadInfo uploadInfo = TextureHelpers::getTextureUploadInfo(textureData);

    switch (textureTarget) {
        case vg::TextureTarget::TEXTURE_1D:
        case vg::TextureTarget::PROXY_TEXTURE_1D:
            glTextureStorage1D(handle, mipmapLevels, (VGEnum)uploadInfo.internalFormat, dims.x);
            glTextureSubImage1D(handle, 0, 0, dims.x, (VGEnum)uploadInfo.textureFormat, (VGEnum)uploadInfo.texturePixelType, textureData.data());
            break;
        case vg::TextureTarget::TEXTURE_2D:
            glTextureStorage2D(handle, mipmapLevels, (VGEnum)uploadInfo.internalFormat, dims.x, dims.y);
            glTextureSubImage2D(handle, 0, 0, 0, dims.x, dims.y, (VGEnum)uploadInfo.textureFormat, (VGEnum)uploadInfo.texturePixelType, textureData.data());
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

GLTexture TextureRepository::uploadDDSTexture(const gli::texture2d& textureData, vg::TextureTarget textureTarget, const vg::SamplerState& samplerState, i32 maxMipLevels)
{
    assert(!textureData.empty());
    const ui32v2 dims(textureData.extent().x, textureData.extent().y);
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    //const i32 mipmapLevels = glm::min(maxMipLevels, (i32)textureData.levels());
    // TODO: GLI IS 1 LESS???
    const i32 mipmapLevels = computeMipmapCount(dims, maxMipLevels);

    VGEnum internalFormat;
    switch (textureData.format()) {
        case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
            internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
            internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
            internalFormat = GL_COMPRESSED_RED_RGTC1;
            break;
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:
            internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
            break;
        default:
            assert(false && "Unsupported format in uploadDDSTexture");
    }

    assert(textureTarget == vg::TextureTarget::TEXTURE_2D);
    glTextureStorage2D(
        handle,
        mipmapLevels,
        internalFormat,
        textureData.extent().x,
        textureData.extent().y
    );

    glCompressedTextureSubImage2D(
        handle,
        0, // mipmap level
        0, 0, // xoffset, yoffset
        textureData.extent().x,
        textureData.extent().y,
        internalFormat,
        static_cast<GLsizei>(textureData.size(0)),
        textureData.data()
    );

    checkGlError("TextureRepository::uploadDDSTexture");
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
