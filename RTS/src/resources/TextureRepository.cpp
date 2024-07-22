#include "stdafx.h"
#include "TextureRepository.h"

#include "rendering/texture/TextureConvert.h"
#include "rendering/texture/TextureHelpers.h"
#include "rendering/texture/MaterialTextureGenerator.h"

#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"

#include "util/TextureUtil.h"

#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

#include "io/PngLoader.h"

#include "filesystem/FileSystem.h"

#include <gli/gli.hpp>
#include <gli/texture.hpp>

#include "gliHasAlpha.inl"


void convertToPremultipliedAlpha(gli::texture2d& texture) {
    if (texture.empty() || !gliHasAlpha(texture.format())) {
        return; // No conversion needed if texture is empty or has no alpha channel
    }

    assert(texture.levels() == 0); // We don't support mipmaps for now

    const gli::texture2d::extent_type extent = texture.extent();
    const gli::texture2d::size_type totalPixels = extent.x * extent.y;

    gli::byte* data = (gli::byte*)texture.data(0, 0, 0 /*level*/);
    const gli::texture2d::size_type pixelSize = gli::detail::bits_per_pixel(texture.format()) / 8;

    switch (texture.format()) {
        case gli::FORMAT_RGBA8_UNORM_PACK8: {
            assert(pixelSize == 1);
            for (gli::texture2d::size_type i = 0; i < totalPixels * 4; i += 4) {
                ui8v4& texel = *(std::bit_cast<ui8v4*>(data + i));
                f32 a = texel.a / 255.0f;
                texel.r = glm::round(texel.r * a);
                texel.g = glm::round(texel.g * a);
                texel.b = glm::round(texel.b * a);
            }
            break;
        }
        default:
            panic("Unhandled gli format {} in convertToPremultipliedAlpha", (int)texture.format());
    }
}

struct TextureLoadUserData {
    gli::texture2d rs; // Optional cached CPU resource data for if we want to query the pixels
    gli::texture2d ddsRs;
};

TextureRepository::TextureRepository(vio::IOManager& ioManager) : IAssetRepository<TextureDef>(ioManager) {
}

TextureRepository::~TextureRepository() = default;

void TextureRepository::init() {
    mNormalMapGenerator = std::make_unique<MaterialTextureGenerator>();
    mNormalMapGenerator->init();
    TextureConvert::initConverters();
}

gli::texture2d TextureRepository::loadRawPngData(AssetID textureId, bool flipV)
{
    return loadRawPngData(getAssetFilePath(textureId), flipV);
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

    return GLTexture(handle, textureTarget, dims, mipmapLevels, uploadInfo.textureFormat);
}

GLTexture TextureRepository::uploadDDSTexture(const gli::texture2d& textureData, vg::TextureTarget textureTarget, const vg::SamplerState& samplerState, i32 maxMipLevels)
{
    assert(!textureData.empty());
    const ui32v2 dims(textureData.extent().x, textureData.extent().y);
    VGTexture handle;
    glCreateTextures((VGEnum)textureTarget, 1, &handle);
    const i32 mipmapLevels = maxMipLevels > 0 ? glm::min(maxMipLevels, (i32)textureData.levels()) : (i32)textureData.levels();
   
    VGEnum internalFormat;
    vg::TextureFormat textureFormat;
    switch (textureData.format()) {
        case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
            internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            textureFormat = vg::TextureFormat::RGB;
            break;
        case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
            internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            textureFormat = vg::TextureFormat::RGBA;
            break;
        case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
            internalFormat = GL_COMPRESSED_RED_RGTC1;
            textureFormat = vg::TextureFormat::RED;
            break;
        case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
            internalFormat = GL_COMPRESSED_RG_RGTC2;
            textureFormat = vg::TextureFormat::RG;
            break;
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:
            internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
            textureFormat = vg::TextureFormat::RGBA;
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

    for (gli::texture2d::size_type level = 0; level < mipmapLevels; ++level) {
        // Get extent of the current mip level
        gli::extent2d levelExtent = textureData.extent(level);

        // Upload the compressed texture data for this mip level to OpenGL
        glCompressedTextureSubImage2D(
            handle,
            static_cast<GLint>(level), // mipmap level
            0, 0, // xoffset, yoffset
            levelExtent.x,
            levelExtent.y,
            internalFormat,
            static_cast<GLsizei>(textureData.size(level)), // size of this mip level
            textureData.data(0, 0, level) // data pointer for this mip level
        );
    }

    checkGlError("TextureRepository::uploadDDSTexture");
    // Setup Texture Sampling Parameters
    samplerState.setForTexture(handle);

    // Mipmap LOD
    if (mipmapLevels > 0) {
        glTextureParameteri(handle, GL_TEXTURE_MAX_LOD, mipmapLevels);
        glTextureParameteri(handle, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
    }

    return GLTexture(handle, textureTarget, dims, mipmapLevels, textureFormat);
}

void TextureRepository::setSamplerState(AssetID textureId, const vg::SamplerState& samplerState) {
    ASSERT_RENDER_THREAD();
    assert(isAssetLoaded(textureId));
    TextureDef& def = *mAssets[textureId];
    if (def.samplerState == &samplerState) {
        return;
    }
    if (def.gpuTexture.isTextureImmutable()) {
        panic("Tried to change sampler on immutable texture {} {}", textureId, mAssetRegistry[textureId].mName.toString());
    }
    def.samplerState = &samplerState;
    if (def.gpuTexture.hasHandle()) {
        samplerState.setForTexture(def.gpuTexture.getHandle());
    }
}

AssetLoadFunc TextureRepository::getAssetLoadFunc() {

    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {

        TextureDef& textureDef = *static_cast<TextureDef*>(assetDataPtr);
        TextureLoadUserData& loadUserData = std::any_cast<TextureLoadUserData&>(userData);

        // Default properties
        textureDef.samplerState = &vg::sSamplerStates.LINEAR_WRAP_MIPMAP;
        textureDef.flipV = false;

        // If this is registered as a material, use the material sampler state
        if (const MaterialDef* materialDef = MaterialRepository::get().tryGetLoadedOrUnloadedAsset(textureDef.getName())) {
            textureDef.samplerState = &vg::sSamplerStates.STATE_ARRAY[e_cast(materialDef->samplerState)];
        }

        LOG_INFO("Loading texture {}", textureDef.getName().toString().c_str());

        // Get absolute path of texture.
        vio::Path texPath;
        mIoManager.resolvePath(filePath, texPath);
        ResourceManager& resourceManager = ResourceManager::get();
        const fs::path resourceRoot(resourceManager.getResourceRoot().getString());
        const fs::path& cacheRoot(resourceManager.getCacheRoot().getString());

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
            ddsPath = cacheRoot / ddsPath;

            if (fs::is_regular_file(ddsPath)) {
                if (FileSystem::getLastFileWriteTime(ddsPath) >= fileLastWriteTime) {
                    needsGenerateDDS = false;
                }
            }

            if (needsGenerateDDS/* || outRs*/) {
                loadUserData.rs = PngLoader::loadPng(stdPath, textureDef.flipV);
                // UI assets are premultiplied alpha
                if (vio::containsSubpath(stdPath, "data\\ui")) {
                    convertToPremultipliedAlpha(loadUserData.rs);
                }

                loadUserData.ddsRs = TextureConvert::convertToDDS(loadUserData.rs, true /*generateMipmaps*/);

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

                if (gli::save(loadUserData.ddsRs, ddsPath.string())) {
                    LOG_TRACE("  Done");
                }
                else {
                    panic("  FAILED! Ensure {} directory exists or ensure program has permission to create folders", ddsPath.string().c_str());
                }

            }
            else {
                // Assume 2d texture (potentially unsafe?)
                LOG_INFO("  Loading cached dds");
                loadUserData.ddsRs = static_cast<gli::texture2d>(gli::load(ddsPath.string()));
            }
        }
        else {
            assert(false);
        }
        return true;
    };
}


AssetLoadFunc TextureRepository::getAssetLoadRenderProcessFunc() {

    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        TextureDef& textureDef = *static_cast<TextureDef*>(assetDataPtr);
        TextureLoadUserData& loadUserData = std::any_cast<TextureLoadUserData&>(userData);

        // TODO: Evaluate if we should always be using this. This fixes crash when dimensions are not divisible by 4
       // TODO: MOVE
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Handle weird texture dimensions

        // Prefer dds
        if (loadUserData.ddsRs.size()) {
            textureDef.gpuTexture = uploadDDSTexture(loadUserData.ddsRs, vg::TextureTarget::TEXTURE_2D, *textureDef.samplerState, INT_MAX);
        }
        else if (loadUserData.rs.size()) {
            textureDef.gpuTexture = uploadTexture(loadUserData.rs, vg::TextureTarget::TEXTURE_2D, *textureDef.samplerState, INT_MAX);
        }
        return true;
    };
}

std::any TextureRepository::getUserData(AssetID) {
    return TextureLoadUserData();
}