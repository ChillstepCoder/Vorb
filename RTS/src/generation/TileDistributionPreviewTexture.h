#pragma once

class TileDistributionDef;
class MaterialShaderDef;

class TileDistributionPreviewTexture {
public:
    TileDistributionPreviewTexture() = default;
    ~TileDistributionPreviewTexture();

    static constexpr i32 MIN_TEXTURE_SIZE = 64;
    static constexpr i32 MAX_TEXTURE_SIZE = 512;

    void generate(const TileDistributionDef& def, i32 textureSize, i32v2 tilePosCorner, f32 densityMult);
    void renderFullScreen(bool showSpawns, bool showThreshold);

    VGTexture mThresholdTexture = 0;
    VGTexture mSpawnTexture = 0;
    i32 mTextureSize = 0;
    AssetHandlePtr<MaterialShaderDef> mImageShader;
};