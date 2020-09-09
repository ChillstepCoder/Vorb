#pragma once

// TODO: ModPathResolver?

#include <Vorb/io/Path.h>

#include "SpriteData.h"

DECL_VVOX(class VoxelTextureStitcher);

class ResourceManager;
struct SpriteData;

class TileSpriteLoader {
public:
    TileSpriteLoader(ResourceManager& resourceManager);
    ~TileSpriteLoader();

    bool loadSpriteSheet(const vio::Path& filePath);
    bool loadSpriteSheetToAtlas(const vio::Path& filePath);

private:

    struct SpriteMetaData {
        ui16v4 pixelRect;
        TileTextureMethod method = TileTextureMethod::SIMPLE;
        std::string textureName;
    };

    // If no meta file, returns  default sprite data
    void buildSpriteDataFromMetaFile(const vio::Path& imageFilePath, const ui32v2& fileDims, OUT std::vector<SpriteMetaData>& metaDataArray);
    std::string getTextureNameFromFilePath(const vio::Path& path);

    ResourceManager& mResourceManager;
    std::unique_ptr<vvox::VoxelTextureStitcher> textureMapper;
};

