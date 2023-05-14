#pragma once
// NEW

#include <Vorb/graphics/SamplerStateType.h>
#include <Vorb/graphics/GLEnums.h>
#include "rendering/texture/GLTexture.h"
#include "rendering/texture/Cubemap.h"

// TODO: DECL_GLI
namespace gli {
    class texture2d;
}

DECL_VG(class SamplerState);
DECL_VIO(class IOManager);

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

    const TextureData* loadTexture(const vio::Path& filePath, vg::TextureTarget type, const vg::SamplerState* samplerState, bool flipV, gli::texture2d* outRs = nullptr);
    const TextureData& getTexture(const nString& textureName) const;

    const Cubemap* loadCubemap(const vio::Path& cubeFilePath);
    const Cubemap& getCubemap(const nString& cubemapName) const;
    const Cubemap& getCubemap(CubemapID cubemapId) const;
    const std::map<nString, CubemapID>& getCubemapIDs() const { return mCubemapIdLookup; }

    // Loads in as RGBAUI8
    gli::texture2d loadRawPngData(const vio::Path& filePath, bool flipV);

private:

    GLTexture uploadTexture(
        const gli::texture2d& textureData,
        vg::TextureTarget textureTarget,
        const vg::SamplerState& samplerState,
        i32 maxMipLevels);
    GLTexture uploadDDSTexture(
        const gli::texture2d& textureData,
        vg::TextureTarget textureTarget,
        const vg::SamplerState& samplerState,
        i32 maxMipLevels);


    // NEW
    std::vector<TextureData> mTextures;
    std::map<nString, TextureID> mTextureIdLookup;
    std::vector<std::unique_ptr<Cubemap>> mCubemaps;
    std::map<nString, CubemapID> mCubemapIdLookup;

    vio::IOManager& mIoManager;
    std::unique_ptr<MaterialTextureGenerator> mNormalMapGenerator;
    nString mDataBuffer;
};
