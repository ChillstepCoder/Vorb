#pragma once
// NEW

#include "resources/IAssetRepository.h"
#include "definitions/rendering/TextureDef.h"

DECL_VG(class SamplerState);

class MaterialTextureGenerator;

// TODO: https://github.com/g-truc/gli/blob/master/manual.md
// STB_DXT
class TextureRepository : public IAssetRepository<TextureDef> {
    ASSET_REPOSITORY_COMMON_CODE(TextureRepository, TextureDef, AssetType::Texture)

    // Loads in as RGBAUI8
    gli::texture2d loadRawPngData(const vio::Path& filePath, bool flipV);

    GLTexture uploadTexture(
        const gli::texture2d& textureData,
        vg::TextureTarget textureTarget,
        const vg::SamplerState& samplerState,
        i32 maxMipLevels = INT_MAX);
    GLTexture uploadDDSTexture(
        const gli::texture2d& textureData,
        vg::TextureTarget textureTarget,
        const vg::SamplerState& samplerState,
        i32 maxMipLevels = INT_MAX);

    bool saveAsset(AssetID assetId) override { panic("Cannot save textures yet"); }

protected:
    void initInternal() override;
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
private:
    std::unique_ptr<MaterialTextureGenerator> mNormalMapGenerator;
    nString mDataBuffer;
};