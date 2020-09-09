#include "stdafx.h"
#include "TileSpriteLoader.h"
#include "ResourceManager.h"

#include "rendering/TextureAtlas.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/voxel/VoxelTextureStitcher.h>
// TODO: Remove?
#include <Vorb/graphics/TextureCache.h>

// Must be power of 2 and >= 16
const unsigned MIN_TILE_SIZE = 16;

TileSpriteLoader::TileSpriteLoader(ResourceManager& resourceManager) :
    mResourceManager(resourceManager)
{
    textureMapper = std::make_unique<vvox::VoxelTextureStitcher>(TEXTURE_ATLAS_SIZE / MIN_TILE_SIZE);
}

TileSpriteLoader::~TileSpriteLoader()
{

}

bool TileSpriteLoader::loadSpriteSheet(const vio::Path& filePath) {
    // Load texture
    vg::Texture texture = mResourceManager.mTextureCache->addTexture(
        filePath,
        vg::TextureTarget::TEXTURE_2D,
        &vg::SamplerState::LINEAR_CLAMP_MIPMAP,
        vg::TextureInternalFormat::RGBA,
        vg::TextureFormat::RGBA
    );

    std::vector<SpriteMetaData> metaDataArray;
    buildSpriteDataFromMetaFile(filePath, ui32v2(texture.width, texture.height), metaDataArray);
    for (auto&& metaData : metaDataArray) {
        SpriteData sprite;
        sprite.texture = texture.id;

        sprite.uvs.x = ((float)metaData.pixelRect.x / texture.width);
        sprite.uvs.y = ((float)metaData.pixelRect.y / texture.height);
        sprite.uvs.z = ((float)metaData.pixelRect.z / texture.width);
        sprite.uvs.w = ((float)metaData.pixelRect.w / texture.height);

        if (metaData.textureName.size()) {
            auto it = mResourceManager.mSprites.find(metaData.textureName);
            if (it != mResourceManager.mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mResourceManager.mSprites.insert(std::make_pair(std::move(metaData.textureName), std::move(sprite)));
        }
        else {
            std::string textureName = getTextureNameFromFilePath(filePath);
            auto it = mResourceManager.mSprites.find(textureName);
            if (it != mResourceManager.mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mResourceManager.mSprites.insert(std::make_pair(std::move(textureName), std::move(sprite)));
        }
    }
    
    return true;
}

bool TileSpriteLoader::loadSpriteSheetToAtlas(const vio::Path& filePath) {
    if (!filePath.isValid()) return false;

    vg::ScopedBitmapResource rs(vg::ImageIO().load(filePath, vg::ImageIOFormat::RGBA_UI8));
    assert(rs.width <= TEXTURE_ATLAS_SIZE && rs.height <= TEXTURE_ATLAS_SIZE);
    assert(rs.width % MIN_TILE_SIZE == 0 && rs.height % MIN_TILE_SIZE == 0);

    std::vector<SpriteMetaData> spriteData;
    buildSpriteDataFromMetaFile(filePath, ui32v2(rs.width, rs.height), spriteData);
    for (auto&& sprite : spriteData) {

        ui32v2 tileDims(rs.width / MIN_TILE_SIZE, rs.height / MIN_TILE_SIZE);

        ui32 index;
        if (tileDims.x == 1 && tileDims.y == 1) {
         //   textureMapper->mapBox()
        }

    }
    //layer.index.layer = m_texturePack->addLayer(layer, layer.path, (color4*)rs.bytesUI8v4);

    return false;
}

void TileSpriteLoader::buildSpriteDataFromMetaFile(const vio::Path& imageFilePath, const ui32v2& fileDims, OUT std::vector<SpriteMetaData>& metaDataArray)
{
    // Find meta file
    std::string metaFilePath = imageFilePath.getString();
    metaFilePath.resize(metaFilePath.size() - 4); // Chop of .png
    metaFilePath += ".meta";
    vio::File metaFile;
    const bool asFile = vio::Path(metaFilePath).asFile(&metaFile);
    assert(asFile);
    vio::FileStream fs = metaFile.open();

    // Meta file is optional, and describes sprites
    if (fs.isOpened()) {

        // Read meta data
        i32v2 tileDims;
        if (fs.read_s("%d,%d", &tileDims.x, &tileDims.y) != 2) {
            char buf[16];
            if (fs.read_s("%15s", buf, sizeof(buf))) {
                SpriteMetaData newData;
                if (strcmp(buf, "con_wall") == 0) {
                    newData.method = TileTextureMethod::CONNECTED_WALL;
                }
                else if (strcmp(buf, "con") == 0) {
                    newData.method = TileTextureMethod::CONNECTED;
                }
                metaDataArray.emplace_back(std::move(newData));
                return;
            }
            else {
                assert(false); // Bad meta file
                return;
            }
        }

        assert(fileDims.x % tileDims.x == 0 && fileDims.y % tileDims.y == 0);

        const ui32 tilePixelWidth = fileDims.x / tileDims.x;
        const ui32 tilePixelHeight = fileDims.y / tileDims.y;

        char textureName[128];
        i32v2 tilePos;
        i32v2 rectSize;
        while (fs.read_s("%127s %d,%d %d,%d", textureName, sizeof(textureName), &tilePos.x, &tilePos.y, &rectSize.x, &rectSize.y) == 5) {
            SpriteMetaData newData;
            newData.pixelRect.x = tilePos.x * tilePixelWidth;
            newData.pixelRect.y = tilePos.y * tilePixelHeight;
            newData.pixelRect.z = rectSize.x * tilePixelWidth;
            newData.pixelRect.w = rectSize.y * tilePixelHeight;
            newData.textureName = textureName;
            metaDataArray.emplace_back(std::move(newData));
        }
    }
    return;
}

std::string TileSpriteLoader::getTextureNameFromFilePath(const vio::Path& path) {
    std::string textureName = path.getLeaf();
    textureName.resize(textureName.size() - 4); // Chop off .png
    return textureName;
}
