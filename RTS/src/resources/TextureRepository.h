#pragma once
// NEW

#include <Vorb/graphics/SamplerStateType.h>
#include "rendering/texture/SubTexture.h"

DECL_VG(class TextureCache);
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

class TextureRepository {
public:
    TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager);
    ~TextureRepository();

    bool loadTexture(const vio::Path& filePath);
    const SubTexture& getTexture(const nString& textureName) const;

private:
    SubTexture& newSubTexture(const nString& name, VGTexture diffuse, TextureHandle diffuseHandle, VGTexture normal, TextureHandle normalHandle, const f32v4& uvRect, bool randFlip);
    TextureMetaData getFileMetadata(const vio::Path& imageFilePath);

    std::vector<SubTexture> mSubTextures;
    std::map<nString, SubTextureID> mTextureIdLookup;
    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    std::unique_ptr<NormalMapGenerator> mNormalMapGenerator;
    nString mDataBuffer;
};
