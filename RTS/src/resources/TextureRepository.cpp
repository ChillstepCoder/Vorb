#include "stdafx.h"
#include "TextureRepository.h"

#include "rendering/texture/NormalMapGenerator.h"

#include <Vorb/graphics/TextureCache.h>

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

KEG_TYPE_DEF(SubtextureMetaData, SubtextureMetaData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(SubtextureMetaData, name), keg::BasicType::STRING));
    kt.addValue("uv_rect", keg::Value::basic(offsetof(SubtextureMetaData, uvRect), keg::BasicType::F32_V4));
    kt.addValue("pixel_rect", keg::Value::basic(offsetof(SubtextureMetaData, pixelRect), keg::BasicType::UI32_V4));
    kt.addValue("rand_flip", keg::Value::basic(offsetof(SubtextureMetaData, randFlip), keg::BasicType::BOOL));
}

KEG_TYPE_DEF(TextureMetaData, TextureMetaData, kt) {
    kt.addValue("sampler_state", keg::Value::custom(offsetof(TextureMetaData, samplerState), "SamplerStateType", true));
    kt.addValue("textures", keg::Value::array(offsetof(TextureMetaData, subTextures), keg::Value::custom(0, "SubtextureMetaData", false)));
    kt.addValue("flipv", keg::Value::basic(offsetof(TextureMetaData, flipV), keg::BasicType::BOOL));
}

TextureRepository::TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager) : mTextureCache(textureCache), mIoManager(ioManager) {
    mNormalMapGenerator = std::make_unique<NormalMapGenerator>();
    mNormalMapGenerator->init();
}

TextureRepository::~TextureRepository() {

}

bool TextureRepository::loadTexture(const vio::Path& filePath) {
    // TODO: Test using temporary nString buffer memory so we dont keep heap allocating all these strings
    nString textureName = vio::getLeafNameFromFilePathNoExtension(filePath);

    // Get any metadata
    TextureMetaData metaData = getFileMetadata(filePath);

    // Load and add texture to cache
    vg::Texture texture = mTextureCache.addTexture(filePath, textureName, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.STATE_ARRAY[e_cast(metaData.samplerState)], vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, !metaData.flipV);
    const nString pathNoExtension = getStringNoExtension(filePath);

    // Check if there is an acompanying stencil file
    //VGTexture stencilTexture;
    //vio::Path stencilTexturePath = pathNoExtension + ".sten.png";
    //if (mIoManager.fileExists(stencilTexturePath)) {
    //    vg::Texture stencilTexture = mTextureCache.addTexture(stencilTexturePath, vio::getLeafNameFromFilePathNoExtension(stencilTexturePath), vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.STATE_ARRAY[e_cast(metaData.samplerState)], vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, !metaData.flipV);
    //


    //    mTextureCache.freeTexture(stencilTexturePath);
    //}

    // Check if there is an acompanying normal file
    VGTexture normalTexture;
    vio::Path normalTexturePath = pathNoExtension + ".norm.png";
    if (mIoManager.fileExists(normalTexturePath)) {
        // Read the normals
        vg::Texture normalTextureFull = mTextureCache.addTexture(normalTexturePath, vio::getLeafNameFromFilePathNoExtension(normalTexturePath), vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.STATE_ARRAY[e_cast(metaData.samplerState)], vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, !metaData.flipV);
        assert(normalTextureFull.dims == texture.dims);
        normalTexture = normalTextureFull.id;
    }
    else {
        // Generate the normals
        normalTexture = mNormalMapGenerator->generateNormalTexture(texture.id, texture.dims, vg::sSamplerStates.STATE_ARRAY[e_cast(metaData.samplerState)]);
    }

    assert(texture.id && normalTexture);
     // Generate bindless handle
    TextureHandle handleDiffuse = glGetTextureHandleARB(texture.id);
    TextureHandle handleNormal = glGetTextureHandleARB(normalTexture);
    assert(handleDiffuse && handleNormal);
    // Residency means it is now fixed in active memory
    // TODO: Only textures referenced by tiles need to be resident?
    glMakeTextureHandleResidentARB(handleDiffuse);
    glMakeTextureHandleResidentARB(handleNormal);

    // Post processing to set UV rects on subtextures if needed
    constexpr ui32v4 ZERO_VEC{};
    for (unsigned i = 0; i < metaData.subTextures.size(); ++i) {
        SubtextureMetaData& data = metaData.subTextures[i];
        if (data.pixelRect != ZERO_VEC) {
            // Pixels were specified, calculate UVs
            data.uvRect.x = data.pixelRect.x / (f32)texture.width;
            data.uvRect.y = data.pixelRect.y / (f32)texture.height;
            data.uvRect.z = data.pixelRect.z / (f32)texture.width;
            data.uvRect.w = data.pixelRect.w / (f32)texture.height;
        }
    }

    if (metaData.subTextures.size()) {
        for (size_t i = 0; i < metaData.subTextures.size(); ++i) {
            SubtextureMetaData& subTextureData = metaData.subTextures[i];
            newSubTexture(subTextureData.name, texture.id, handleDiffuse, normalTexture, handleNormal, subTextureData.uvRect, subTextureData.randFlip);
        }
    }
    else {
        // No subtextures, so make a single one
        newSubTexture(textureName, texture.id, handleDiffuse, normalTexture, handleNormal, f32v4(0.0f, 0.0f, 1.0f, 1.0f), false /*TODO: hmmmm shouldnt this be allowed*/);
    }

    checkGlError("TextureRepository::loadTexture");

    return true;
}

SubTexture& TextureRepository::newSubTexture(const nString& name, VGTexture diffuse, TextureHandle diffuseHandle, VGTexture normal, TextureHandle normalHandle, const f32v4& uvRect, bool randFlip) {
    SubTexture& newTexture = mSubTextures.emplace_back();
    newTexture.mId = mSubTextures.size() - 1;
    newTexture.mUvRect = uvRect;
    newTexture.mTextureDiffuse = diffuse;
    newTexture.mTextureHandleDiffuse = diffuseHandle;
    newTexture.mTextureNormal = normal;
    newTexture.mTextureHandleNormal = normalHandle;
    if (randFlip) newTexture.mFlags.setBit(SubTextureFlags::RAND_FLIP);

    assert(mTextureIdLookup.find(name) == mTextureIdLookup.end() && "Duplicate texture name detected");
    mTextureIdLookup[name] = newTexture.mId;

    return newTexture;
}

TextureMetaData TextureRepository::getFileMetadata(const vio::Path& imageFilePath)
{
    TextureMetaData metaData;
    // Find meta file
    std::string metaFilePath = imageFilePath.getString();
    metaFilePath.resize(metaFilePath.size() - 4); // Chop off .png
    metaFilePath += ".meta";

    // Meta file is optional, and describes sprites
    if (mIoManager.fileExists(metaFilePath)) {
        // Read file
        mIoManager.readFileToString(metaFilePath, mDataBuffer);
        if (mDataBuffer.empty()) return metaData;

        // Convert to YAML
        keg::ReadContext context;
        context.env = keg::getGlobalEnvironment();
        context.reader.init(mDataBuffer.c_str());
        keg::Node rootObject = context.reader.getFirst();

        try {
            keg::Error error = keg::parse((ui8*)&metaData, rootObject, context, &KEG_GLOBAL_TYPE(TextureMetaData));
            assert(error == keg::Error::NONE);
        }
        catch (YAML::ParserException e) {
            printf("%s : Parser exception %s at line %d column %d pos %d\n", metaFilePath.c_str(), e.msg.c_str(), e.mark.line, e.mark.column, e.mark.pos);
            assert(false);
        }
        catch (YAML::RepresentationException e) {
            printf("%s : Representation exception %s at line %d column %d pos %d\n", metaFilePath.c_str(), e.msg.c_str(), e.mark.line, e.mark.column, e.mark.pos);
            assert(false);
        }
    }

    return metaData;
}


const SubTexture& TextureRepository::getTexture(const nString& textureName) const {
    auto&& it = mTextureIdLookup.find(textureName);
    if (it == mTextureIdLookup.end()) {
        pError("Failed to find texture - " + textureName);
        assert(false && "Texture lookup error");
    }
    return mSubTextures[it->second];
}
