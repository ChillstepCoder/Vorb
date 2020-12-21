#include "stdafx.h"
#include "TileSpriteLoader.h"

#include "rendering/SpriteRepository.h"
#include "rendering/TextureAtlas.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/voxel/VoxelTextureStitcher.h>
// TODO: Remove?
#include <Vorb/graphics/TextureCache.h>

// exception handling
#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"


KEG_TYPE_DEF(SpriteMetaData, SpriteMetaData, kt) {
    kt.addValue("method", keg::Value::custom(offsetof(SpriteMetaData, method), "TileTextureMethod", true));
    kt.addValue("cell_rect", keg::Value::basic(offsetof(SpriteMetaData, cellRect), keg::BasicType::UI8_V4));
    kt.addValue("dims_meters", keg::Value::basic(offsetof(SpriteMetaData, dimsMeters), keg::BasicType::F32_V2));
    kt.addValue("name", keg::Value::basic(offsetof(SpriteMetaData, name), keg::BasicType::STRING));
    kt.addValue("lod_color", keg::Value::basic(offsetof(SpriteMetaData, lodColor), keg::BasicType::UI8_V3));
}

KEG_TYPE_DEF(SpritesheetFileData, SpritesheetFileData, kt) {
    kt.addValue("dims_cells", keg::Value::basic(offsetof(SpritesheetFileData, fileDimsCells), keg::BasicType::UI32_V2));
    kt.addValue("tiles", keg::Value::array(offsetof(SpritesheetFileData, spriteMetaData), keg::Value::custom(0, "SpriteMetaData", false)));
}


TileSpriteLoader::TileSpriteLoader(SpriteRepository& spriteRepository, TextureAtlas& textureAtlas, const vio::IOManager& ioManager) :
    mSpriteRepository(spriteRepository),
    mTextureAtlas(textureAtlas),
    mIoManager(ioManager)
{
    mTextureMapper = std::make_unique<vvox::VoxelTextureStitcher>(TEXTURE_ATLAS_WIDTH_PX / TEXTURE_ATLAS_CELL_WIDTH_PX);
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

    // Create sprite data for each discovered sprite
    for (unsigned i = 0; i < sheetMetaData.spriteMetaData.size(); ++i) {
        SpriteMetaData& metaData = sheetMetaData.spriteMetaData[i];
        SpriteData sprite;
        sprite.texture = mTextureAtlas.getAtlasTexture();
        sprite.dimsMeters = metaData.dimsMeters;
        sprite.lodColor = metaData.lodColor;

        // Determine how many tiles we need to map to the atlas, by finding the AABB in tile units
        // TODO: Ensure this is correct usage of pixelRect.zw
        const ui32v2 tileDims(ceil((float)metaData.pixelRect.z / TEXTURE_ATLAS_CELL_WIDTH_PX), ceil((float)metaData.pixelRect.w / TEXTURE_ATLAS_CELL_WIDTH_PX));
        assert(tileDims.x * tileDims.y > 0);
        assert(tileDims.x < TEXTURE_ATLAS_CELLS_PER_ROW / 2); // Need room for normal maps
        const unsigned sourceOffset = (unsigned)metaData.pixelRect.y * rs.width + metaData.pixelRect.x;
        const color4* sourceBytes = (color4*)(rs.bytesUI8v4 + sourceOffset);

        if (metaData.method == TileTextureMethod::SIMPLE) {
            ui32 index = mTextureMapper->mapBox(tileDims.x, tileDims.y);
            sprite.uvs = mTextureAtlas.writePixels(index, tileDims.x, tileDims.y, sourceBytes, rs.width);
            sprite.atlasPage = mTextureAtlas.getPageIndexFromCellIndex(index);
        }
        else {
            // Connected and others
            ui32 index = mTextureMapper->mapBox(tileDims.x, tileDims.y);
            sprite.uvs = mTextureAtlas.writePixels(index, tileDims.x, tileDims.y, sourceBytes, rs.width);
            sprite.atlasPage = mTextureAtlas.getPageIndexFromCellIndex(index);
            sprite.method = TileTextureMethod::CONNECTED_WALL;
            // Correct dims per tile
            sprite.uvs.z /= TILE_TEX_METHOD_CONNECTED_WALL_WIDTH;
            sprite.uvs.w /= TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT;
        }

        // Insert the sprite
        if (metaData.name.size()) {
            auto it = mSpriteRepository.mSprites.find(metaData.name);
            if (it != mSpriteRepository.mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mSpriteRepository.mSprites.insert(std::make_pair(std::move(metaData.name), std::move(sprite)));
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
    // Some useful data
    metaData.fileDimsPx = ui32v2(fileDimsPx.x, fileDimsPx.y);

    // Find meta file
    std::string metaFilePath = imageFilePath.getString();
    metaFilePath.resize(metaFilePath.size() - 4); // Chop of .png
    metaFilePath += ".meta";

    // Meta file is optional, and describes sprites
    if (mIoManager.fileExists(metaFilePath)) {
        // Read file
        nString data;
        mIoManager.readFileToString(metaFilePath, data);
        if (data.empty()) return;

        // Convert to YAML
        keg::ReadContext context;
        context.env = keg::getGlobalEnvironment();
        context.reader.init(data.c_str());
        keg::Node rootObject = context.reader.getFirst();

        try {
            keg::Error error = keg::parse((ui8*)&metaData, rootObject, context, &KEG_GLOBAL_TYPE(SpritesheetFileData));
            assert(error == keg::Error::NONE);
        }
        catch (YAML::ParserException e) {
            printf("%s : Parser exception %s at line %d column %d pos %d\n", metaFilePath.c_str(), e.msg.c_str(), e.mark.line, e.mark.column, e.mark.pos);
            assert(false);
        }
        catch (YAML::RepresentationException e) {
            printf("%s : Representation exception %s at line %d column %d pos %d\n", metaFilePath.c_str(), e.msg.c_str(), e.mark.line, e.mark.column, e.mark.pos);
            assert(false);
        }

        // Post processing
        metaData.cellDimsPx = metaData.fileDimsCells / metaData.fileDimsPx;
        const ui32 tilePixelWidth = metaData.fileDimsPx.x / metaData.fileDimsCells.x;
        const ui32 tilePixelHeight = metaData.fileDimsPx.y / metaData.fileDimsCells.y;
        for (unsigned i = 0; i < metaData.spriteMetaData.size(); ++i) {
            SpriteMetaData& data = metaData.spriteMetaData[i];
            data.pixelRect.x = data.cellRect.x * tilePixelWidth;
            data.pixelRect.y = data.cellRect.y * tilePixelHeight;
            data.pixelRect.z = data.cellRect.z * tilePixelWidth;
            data.pixelRect.w = data.cellRect.w * tilePixelHeight;
        }
    }
    else {
        // No meta file, treat entire png as a single image
        SpriteMetaData * newData = new SpriteMetaData;
        newData->cellRect = ui16v4(0, 0, 1, 1);
        newData->pixelRect = ui16v4(0, 0, fileDimsPx.x, fileDimsPx.y);
        newData->name = getTextureNameFromFilePath(imageFilePath);
        newData->dimsMeters = f32v2(fileDimsPx) / 16.0f; // Arbitrary
        metaData.spriteMetaData.setData(newData, 1);
        metaData.cellDimsPx = metaData.fileDimsPx;
    }
    return;
}

std::string TileSpriteLoader::getTextureNameFromFilePath(const vio::Path& path) {
    std::string textureName = path.getLeaf();
    textureName.resize(textureName.size() - 4); // Chop off .png
    return textureName;
}
