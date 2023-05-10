#pragma once
// NEW

#include <Vorb/graphics/SamplerStateType.h>
#include <Vorb/graphics/GLEnums.h>
#include "rendering/texture/GLTexture.h"
#include "rendering/texture/Cubemap.h"

DECL_VG(class SamplerState);
DECL_VIO(class IOManager);
DECL_VG(class ScopedBitmapResource);

class MaterialTextureGenerator;

struct TextureData {
    GLTexture texture;
    TextureID textureId;
    vg::TextureTarget type;
    vio::Path texturePath;
    const vg::SamplerState* samplerState;
    bool flipV; // TODO: Flags
};

// TODO: https://github.com/g-truc/gli/blob/master/manual.md
// STB_DXT
class TextureRepository {
public:
    TextureRepository(vio::IOManager& ioManager);
    ~TextureRepository();

    const TextureData* loadTexture(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, vg::TextureInternalFormat internalFormat, bool flipV, vg::ScopedBitmapResource* outRs = nullptr);
    const TextureData& getTexture(const nString& textureName) const;

    const Cubemap* loadCubemap(const vio::Path& cubeFilePath);
    const Cubemap& getCubemap(const nString& cubemapName) const;
    const Cubemap& getCubemap(CubemapID cubemapId) const;
    const std::map<nString, CubemapID>& getCubemapIDs() const { return mCubemapIdLookup; }

    void setTextureAssetPaths(const std::vector<vio::Path>& paths);

    // Loads in as RGBAUI8
    bool loadRawTextureData(const vio::Path& filePath, OUT vg::ScopedBitmapResource& outRs, bool flipV);

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


    // NEW
    std::vector<TextureData> mTextures;
    std::map<nString, TextureID> mTextureIdLookup;
    std::map<nString, TextureID> mTextureAssetPaths;
    std::vector<std::unique_ptr<Cubemap>> mCubemaps;
    std::map<nString, CubemapID> mCubemapIdLookup;

    vio::IOManager& mIoManager;
    std::unique_ptr<MaterialTextureGenerator> mNormalMapGenerator;
    nString mDataBuffer;
};
