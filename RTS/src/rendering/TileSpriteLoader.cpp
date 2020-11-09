#include "stdafx.h"
#include "TileSpriteLoader.h"

#include "rendering/SpriteRepository.h"
#include "rendering/TextureAtlas.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/voxel/VoxelTextureStitcher.h>
// TODO: Remove?
#include <Vorb/graphics/TextureCache.h>

#define ENABLE_TEXTURE_ATLAS 1

ui32 TileSpriteLoader::AtlasTileMapping::getX() const {
    return index % TEXTURE_ATLAS_CELLS_PER_ROW;
}

ui32 TileSpriteLoader::AtlasTileMapping::getY() const {
    return (index % TEXTURE_ATLAS_CELLS_PER_PAGE) / TEXTURE_ATLAS_CELLS_PER_ROW;

}

ui32 TileSpriteLoader::AtlasTileMapping::getPage() const {
    return index / TEXTURE_ATLAS_CELLS_PER_PAGE;
}

TileSpriteLoader::TileSpriteLoader(SpriteRepository& spriteRepository, TextureAtlas& textureAtlas) :
    mSpriteRepository(spriteRepository),
    mTextureAtlas(textureAtlas)
{
    textureMapper = std::make_unique<vvox::VoxelTextureStitcher>(TEXTURE_ATLAS_WIDTH_PX / TEXTURE_ATLAS_CELL_WIDTH_PX);
}

TileSpriteLoader::~TileSpriteLoader()
{

}

bool TileSpriteLoader::loadSpriteTexture(const vio::Path& filePath) {
    if (!filePath.isValid()) return false;

    vg::ScopedBitmapResource rs(vg::ImageIO().load(filePath, vg::ImageIOFormat::RGBA_UI8));
    if (rs.width > TEXTURE_ATLAS_WIDTH_PX || rs.height > TEXTURE_ATLAS_WIDTH_PX) {
        pError("Failed to load " + filePath.getString() + " texture must be smaller than " + std::to_string(TEXTURE_ATLAS_WIDTH_PX));
        return false;
    }
    if (!(rs.width % TEXTURE_ATLAS_CELL_WIDTH_PX == 0 && rs.height % TEXTURE_ATLAS_CELL_WIDTH_PX == 0)) {
        std::cerr << "Failed to load " << filePath.getString() << " dimensions must be divisible by " << std::to_string(TEXTURE_ATLAS_CELL_WIDTH_PX) << "\n";
        return false;
    }

    SpritesheetFileData sheetMetaData;
    getFileMetadata(filePath, ui32v2(rs.width, rs.height), sheetMetaData);

    for (auto&& metaData : sheetMetaData.spriteMetaData) {

        SpriteData sprite;
        sprite.texture = mTextureAtlas.getAtlasTexture();
        sprite.dimsMeters = metaData.dimsMeters;

        // Determine how many tiles we need to map to the atlas, by finding the AABB in tile units
        // TODO: Ensure this is correct usage of pixelRect.zw
        const ui32v2 tileDims(ceil((float)metaData.pixelRect.z / TEXTURE_ATLAS_CELL_WIDTH_PX), ceil((float)metaData.pixelRect.w / TEXTURE_ATLAS_CELL_WIDTH_PX));
        assert(tileDims.x * tileDims.y > 0);
        assert(tileDims.x < TEXTURE_ATLAS_CELLS_PER_ROW / 2); // Need room for normal maps
        const unsigned sourceOffset = (unsigned)metaData.pixelRect.y * rs.width + metaData.pixelRect.x;
        const color4* sourceBytes = (color4*)(rs.bytesUI8v4 + sourceOffset);

        // Reserve double space for normal map
        if (tileDims.x == 1 && tileDims.y == 1) {
            ui32 index = textureMapper->mapSingle();
            sprite.uvs = mTextureAtlas.writePixels(index, 1, 1, sourceBytes, rs.width);
            sprite.atlasPage = mTextureAtlas.getPageIndexFromCellIndex(index);
        }
        else if (sheetMetaData.method == TileTextureMethod::SIMPLE) {
            ui32 index = textureMapper->mapBox(tileDims.x, tileDims.y);
            sprite.uvs = mTextureAtlas.writePixels(index, tileDims.x, tileDims.y, sourceBytes, rs.width);
            sprite.atlasPage = mTextureAtlas.getPageIndexFromCellIndex(index);
        }
        else {
            // Connected and others
            ui32 index = textureMapper->mapBox(tileDims.x, tileDims.y);
            sprite.uvs = mTextureAtlas.writePixels(index, tileDims.x, tileDims.y, sourceBytes, rs.width);
            sprite.atlasPage = mTextureAtlas.getPageIndexFromCellIndex(index);
            sprite.method = TileTextureMethod::CONNECTED_WALL;
            // Correct dims per tile
            sprite.uvs.z /= TILE_TEX_METHOD_CONNECTED_WALL_WIDTH;
            sprite.uvs.w /= TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT;
        }

        // Insert the sprite
        if (metaData.textureName.size()) {
            auto it = mSpriteRepository.mSprites.find(metaData.textureName);
            if (it != mSpriteRepository.mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mSpriteRepository.mSprites.insert(std::make_pair(std::move(metaData.textureName), std::move(sprite)));
        }
        else {
            std::string textureName = getTextureNameFromFilePath(filePath);
            auto it = mSpriteRepository.mSprites.find(textureName);
            if (it != mSpriteRepository.mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mSpriteRepository.mSprites.insert(std::make_pair(std::move(textureName), std::move(sprite)));
        }
    }

    return false;
}

void TileSpriteLoader::getFileMetadata(const vio::Path& imageFilePath, const ui32v2& fileDimsPx, OUT SpritesheetFileData& metaData)
{
    // Find meta file
    std::string metaFilePath = imageFilePath.getString();
    metaFilePath.resize(metaFilePath.size() - 4); // Chop of .png
    metaFilePath += ".meta";
    vio::File metaFile;
    const bool asFile = vio::Path(metaFilePath).asFile(&metaFile);
    assert(asFile);
    vio::FileStream fs = metaFile.open();

    // Some useful data
    metaData.fileDimsPx = ui32v2(fileDimsPx.x, fileDimsPx.y);
    metaData.tileDimsPx = metaData.fileDimsTiles / metaData.fileDimsPx;

    // TODO: Keg
    // Meta file is optional, and describes sprites
    if (fs.isOpened()) {

        // Read meta data
        if (fs.read_s("%u,%u", &metaData.fileDimsTiles.x, &metaData.fileDimsTiles.y) != 2) {
            char buf[16];
            if (fs.read_s("%15s", buf, sizeof(buf))) {
                SpriteMetaData newData;
                newData.pixelRect.x = 0;
                newData.pixelRect.y = 0;
                newData.pixelRect.z = metaData.fileDimsPx.x;
                newData.pixelRect.w = metaData.fileDimsPx.y;
                if (strcmp(buf, "con_wall") == 0) {
                    newData.method = TileTextureMethod::CONNECTED_WALL;
                }
                else if (strcmp(buf, "con") == 0) {
                    newData.method = TileTextureMethod::CONNECTED;
                }
                metaData.method = newData.method;
                metaData.spriteMetaData.emplace_back(std::move(newData));
                return;
            }
            else {
                assert(false); // Bad meta file
                return;
            }
        }

        assert(metaData.fileDimsPx.x % metaData.fileDimsTiles.x == 0 && metaData.fileDimsPx.y % metaData.fileDimsTiles.y == 0);

        const ui32 tilePixelWidth = metaData.fileDimsPx.x / metaData.fileDimsTiles.x;
        const ui32 tilePixelHeight = metaData.fileDimsPx.y / metaData.fileDimsTiles.y;

        char textureName[128];
        i32v2 tilePos;
        i32v2 rectSize; //  In this specification, cells are always 1 meter long, but may have variable pixel density
        while (fs.read_s("%127s %d,%d %d,%d", textureName, sizeof(textureName), &tilePos.x, &tilePos.y, &rectSize.x, &rectSize.y) == 5) {
            SpriteMetaData newData;
            newData.pixelRect.x = tilePos.x * tilePixelWidth;
            newData.pixelRect.y = tilePos.y * tilePixelHeight;
            newData.pixelRect.z = rectSize.x * tilePixelWidth;
            newData.pixelRect.w = rectSize.y * tilePixelHeight;
            newData.textureName = textureName;
            newData.dimsMeters = rectSize;
            metaData.spriteMetaData.emplace_back(std::move(newData));
        }
    }
    else {
        // No meta file, treat entire png as a single image
        SpriteMetaData newData;
        newData.pixelRect.x = 0;
        newData.pixelRect.y = 0;
        newData.pixelRect.z = fileDimsPx.x;
        newData.pixelRect.w = fileDimsPx.y;
        newData.textureName = getTextureNameFromFilePath(imageFilePath);
        newData.dimsMeters = f32v2(fileDimsPx) / 16.0f; // Arbitrary
        metaData.spriteMetaData.emplace_back(std::move(newData));
    }
    return;
}

std::string TileSpriteLoader::getTextureNameFromFilePath(const vio::Path& path) {
    std::string textureName = path.getLeaf();
    textureName.resize(textureName.size() - 4); // Chop off .png
    return textureName;
}
