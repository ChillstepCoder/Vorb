#pragma once

// TODO: ModPathResolver?

#include "SpriteData.h"

DECL_VVOX(class VoxelTextureStitcher);
DECL_VIO(class IOManager);

class TextureAtlas;
class SpriteRepository;
struct SpriteData;

struct SpriteMetaData {
    ui8v4 cellRect = ui16v4(0, 0, 1, 1);
    color3 lodColor = NO_LOD_COLOR;
    ui16v4 pixelRect = ui16v4(0);
    f32v2 dimsMeters = f32v2(1.0f); // World size
    TileTextureMethod method = TileTextureMethod::SIMPLE;
    std::string name;
    f32v2 offset = f32v2(0.0f);
    bool randFlip = false;
    bool opaque   = false;
};
KEG_TYPE_DECL(SpriteMetaData);

struct SpritesheetFileData {
    ui32v2 fileDimsCells = ui32v2(1, 1);
    ui32v2 fileDimsPx = ui32v2(0);
    ui32v2 cellDimsPx = ui32v2(0);
    Array<SpriteMetaData> spriteMetaData;
};
KEG_TYPE_DECL(SpritesheetFileData);

class TileSpriteLoader {
public:
    TileSpriteLoader(SpriteRepository& spriteRepository, TextureAtlas& textureAtlas, const vio::IOManager& ioManager);
    ~TileSpriteLoader();

    bool loadSpriteTexture(const vio::Path& filePath);

private:

    // If no meta file, returns  default sprite data
    void getFileMetadata(const vio::Path& imageFilePath, const ui32v2& fileDimsPx, OUT SpritesheetFileData& metaData);
    std::string getTextureNameFromFilePath(const vio::Path& path);

    const vio::IOManager& mIoManager;
    SpriteRepository& mSpriteRepository;
    TextureAtlas& mTextureAtlas;
    std::unique_ptr<vvox::VoxelTextureStitcher> mTextureMapper;
};

