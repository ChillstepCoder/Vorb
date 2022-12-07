#pragma once
// NEW

#include <Vorb/graphics/SamplerStateType.h>
#include <Vorb/graphics/GLEnums.h>
#include "rendering/texture/SubTexture.h"
#include "rendering/texture/GLTexture.h"

DECL_VG(class TextureCache);
DECL_VG(class SamplerState);
DECL_VIO(class IOManager);

class NormalMapGenerator;

struct SubtextureMetaData {
    std::string name;
    f32v4 uvRect = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
    ui32v4 pixelRect = ui32v4(0, 0, 0, 0);
    bool randFlip = false;
};
KEG_TYPE_DECL(SubtextureMetaData);

struct TextureMetaData {
    Array<SubtextureMetaData> subTextures;
    vg::SamplerStateType samplerState = vg::SamplerStateType::LINEAR_WRAP_MIPMAP;
    bool flipV = false;
};
KEG_TYPE_DECL(TextureMetaData);

struct TextureData {
    GLTexture texture;
    TextureID textureId;
    vio::Path texturePath;
    const vg::SamplerState* samplerState;
    bool flipV; // TODO: Flags
};

class TextureRepository {
public:
    TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager);
    ~TextureRepository();

    const TextureData* loadTextureNew(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, bool flipV);
    const TextureData& getTextureNew(const nString& textureName) const;
    void setTextureAssetPaths(const std::vector<vio::Path>& paths);

    bool loadSubTextureOLD(const vio::Path& filePath);
    const SubTexture& getSubTextureOLD(const nString& textureName) const;

private:
    GLTexture uploadTexture(
        const void* data,
        ui32v2 dims,
        vg::TexturePixelType texturePixelType /*= TexturePixelType::UNSIGNED_BYTE*/,
        vg::TextureTarget textureTarget /*= vg::TextureTarget::TEXTURE_2D*/,
        const vg::SamplerState* samplingParameters /*= &SamplerState::LINEAR_CLAMP_MIPMAP*/,
        vg::TextureInternalFormat internalFormat /* = vg::TextureInternalFormat::RGBA*/,
        vg::TextureFormat textureFormat /* = vg::TextureFormat::RGBA */,
        i32 mipmapLevels);

    SubTexture& newSubTexture(const nString& name, VGTexture diffuse, TextureHandle diffuseHandle, VGTexture normal, TextureHandle normalHandle, const f32v4& uvRect, bool randFlip);
    TextureMetaData getFileMetadata(const vio::Path& imageFilePath);

    // NEW
    std::vector<TextureData> mTextures;
    std::map<nString, TextureID> mTextureIdLookup;
    std::map<nString, TextureID> mTextureAssetPaths;

    // OLD
    std::vector<SubTexture> mSubTextures;
    std::map<nString, SubTextureID> mSubTextureIdLookup;
    vg::TextureCache& mTextureCache;

    vio::IOManager& mIoManager;
    std::unique_ptr<NormalMapGenerator> mNormalMapGenerator;
    nString mDataBuffer;
};
