#pragma once
// NEW

#include "resources/IAssetRepository.h"
#include "definitions/rendering/TextureDef.h"

DECL_VG(class SamplerState);

class MaterialTextureGenerator;

// TODO: https://github.com/g-truc/gli/blob/master/manual.md
// STB_DXT
class TextureRepository : public IAssetRepository<TextureDef> {
    ASSET_REPOSITORY_COMMON_CODE_NO_CONSTRUCTOR(TextureRepository, TextureDef, AssetType::Texture)
    TextureRepository(vio::IOManager& ioManager);
    ~TextureRepository();
    void init() override;

    // Loads in as RGBAUI8
    gli::texture2d [[nodiscard]] loadRawPngData(AssetID textureId, bool flipV);
    gli::texture2d [[nodiscard]] loadRawPngData(const vio::Path& filePath, bool flipV);

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
    void setSamplerState(AssetID textureId, const vg::SamplerState& samplerState);

    StrToken getAssetExtension() const override { return CStrToken("png"); }
    const char* const getAssetTypeDisplayName() const override { return "PNG"; }

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
    std::any getUserData(AssetID id) override;
private:
    std::unique_ptr<MaterialTextureGenerator> mNormalMapGenerator;
    nString mDataBuffer;
    float mMaxAniso = 0.0f;
};