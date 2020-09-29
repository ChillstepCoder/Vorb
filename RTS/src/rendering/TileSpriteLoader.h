#pragma once

// TODO: ModPathResolver?

#include <Vorb/io/Path.h>

#include "SpriteData.h"

DECL_VVOX(class VoxelTextureStitcher);

class TextureAtlas;
class SpriteRepository;
struct SpriteData;

class TileSpriteLoader {
public:
    TileSpriteLoader(SpriteRepository& spriteRepository, TextureAtlas& textureAtlas);
    ~TileSpriteLoader();

    bool loadSpriteTexture(const vio::Path& filePath);

private:

    struct SpriteMetaData {
        ui16v4 pixelRect = ui16v4(0);
        f32v2 dimsMeters = f32v2(1.0f); // World size
        TileTextureMethod method = TileTextureMethod::SIMPLE;
        std::string textureName;
    };

    struct SpritesheetFileData {
        std::vector<SpriteMetaData> spriteMetaData;
        TileTextureMethod method = TileTextureMethod::SIMPLE;
        ui32v2 fileDimsTiles = ui32v2(1, 1);
        ui32v2 fileDimsPx = ui32v2(0);
        ui32v2 tileDimsPx = ui32v2(0);
    };

    struct AtlasTileMapping {

        AtlasTileMapping() {};
        AtlasTileMapping(ui32 index) : index(index) {};
        AtlasTileMapping& operator= (ui32 index) { this->index = index; return *this; }

        ui32 getX() const;
        ui32 getY() const;
        ui32 getPage() const;

        ui32 index = 0;
    };

    // If no meta file, returns  default sprite data
    void getFileMetadata(const vio::Path& imageFilePath, const ui32v2& fileDimsPx, OUT SpritesheetFileData& metaData);
    std::string getTextureNameFromFilePath(const vio::Path& path);

    SpriteRepository& mSpriteRepository;
    TextureAtlas& mTextureAtlas;
    std::unique_ptr<vvox::VoxelTextureStitcher> textureMapper;
};

