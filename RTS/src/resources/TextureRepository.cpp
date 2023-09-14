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

void TextureRepository::initInternal() {
    mNormalMapGenerator = std::make_unique<MaterialTextureGenerator>();
    mNormalMapGenerator->init();
    TextureConvert::initConverters();
}

AssetLoadFunc TextureRepository::getAssetLoadFunc() {

    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        TextureDef& textureDef = *static_cast<TextureDef*>(assetDataPtr);

        // Default properties
        textureDef.samplerState = &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP;
        textureDef.type = vg::TextureTarget::TEXTURE_2D;
        textureDef.flipV = true;

        LOG_INFO("Loading texture {}", textureDef.getName().toString().c_str());
        // TODO: asset .meta???
        textureDef.rs = std::make_unique<gli::texture2d>();
        gli::texture2d* rsPtr = textureDef.rs.get();

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

            if (needsGenerateDDS/* || outRs*/) {
                *rsPtr = PngLoader::loadPng(stdPath, textureDef.flipV);

                //if (outRs) {
                //    // If caller requires full data, we wont ever generate dds
                //    texture = uploadTexture(*rsPtr, type, *samplerState, INT_MAX);
                //}
                //else {
                    // Compression
                textureDef.ddsRs = std::make_unique<gli::texture2d>(TextureConvert::convertToDDS(*rsPtr));

                // Cache to disk
                LOG_TRACE("  Saving to disk - {}", ddsPath.string());

                // Ensure directories exist
                fs::path directoryPath = ddsPath;
                directoryPath._Remove_filename_and_separator();


                if (!std::filesystem::exists(directoryPath)) {
                    static std::mutex createDirMutex;
                    std::lock_guard lock(createDirMutex);
                    if (!std::filesystem::create_directories(directoryPath)) {
                        // Maybe another thread succeeded?
                        if (!std::filesystem::exists(directoryPath)) {
                            panic("  Failed to create directories for {}", directoryPath.string());
                        }
                    }
                }

                if (gli::save(*textureDef.ddsRs, ddsPath.string())) {
                    LOG_TRACE("  Done");
                }
                else {
                    panic("  FAILED! Ensure {} directory exists or ensure program has permission to create folders", ddsPath.string().c_str());
                }

            }
            else {
                // Assume 2d texture (potentially unsafe?)
                LOG_INFO("  Loading cached dds");
                textureDef.ddsRs = std::make_unique<gli::texture2d>(gli::load(ddsPath.string()));
            }
        }
        else {
            assert(false);
        }
    };
}


AssetLoadFunc TextureRepository::getAssetLoadRenderProcessFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        TextureDef& textureDef = *static_cast<TextureDef*>(assetDataPtr);

        if (textureDef.ddsRs) {
            textureDef.gpuTexture = uploadDDSTexture(*textureDef.ddsRs, textureDef.type, *textureDef.samplerState, INT_MAX);
            textureDef.rs.reset();
        }
        else if (textureDef.rs) {
            textureDef.gpuTexture = uploadTexture(*textureDef.rs, textureDef.type, *textureDef.samplerState, INT_MAX);
            // Don't discard rs
        }
    };
}