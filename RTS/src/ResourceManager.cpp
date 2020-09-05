#include "stdafx.h"
#include "ResourceManager.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/IO.h>

ResourceManager::ResourceManager() {
    mTextureCache = std::make_unique<vg::TextureCache>();
    mIoManager = std::make_unique<vio::IOManager>();
    mTextureCache->init(mIoManager.get());
}

ResourceManager::~ResourceManager() {

}

bool fileHasExtension(const vio::Path& filePath, const std::string& extension) {
    size_t length = filePath.getString().size();
    if (filePath.getString().size() < extension.size()) {
        return false;
    }
    return strcmp(filePath.getString().c_str() + (length - extension.size()), extension.c_str()) == 0;
}

void ResourceManager::loadResources(const vio::Path& folderPath) {
    vio::Directory directory;
    if (!folderPath.asDirectory(&directory)) {
        // TODO: Better error messaging
        assert(false);
    }

    vio::DirectoryEntries entries;
    if (!directory.appendEntries(entries)) {

        assert(false); // Empty directory
    }

    for (auto&& entry : entries) {
        if (fileHasExtension(entry, ".png")) {
            loadSpriteSheet(entry);
        }
        else if (fileHasExtension(entry, ".tile")) {
            loadTiles(entry);
        }
    }

    mHasLoadedResources = true;
}

SpriteData ResourceManager::getSprite(const std::string& spriteName) {
    SpriteData rv;

    auto it = mSprites.find(spriteName);
    if (it != mSprites.end()) {
        return it->second;
    }

    return rv;
}

// TODO: Clean this up
bool ResourceManager::loadSpriteSheet(const vio::Path& filePath) {
    // Load texture
    vg::Texture texture = mTextureCache->addTexture(
        filePath,
        vg::TextureTarget::TEXTURE_2D,
        &vg::SamplerState::LINEAR_CLAMP_MIPMAP,
        vg::TextureInternalFormat::RGBA,
        vg::TextureFormat::RGBA
    );

    // Find meta file
    std::string metaFilePath = filePath.getString();
    metaFilePath.resize(metaFilePath.size() - 4); // Chop of .png
    metaFilePath += ".meta";
    vio::File metaFile;
    assert(vio::Path(metaFilePath).asFile(&metaFile));
    vio::FileStream fs = metaFile.open();

    // Meta file is optional, and describes sprites
    if (fs.isOpened()) {

        // Read meta data
        i32v2 fileDims;
        if (fs.read_s("%d,%d", &fileDims.x, &fileDims.y) != 2) {
            char buf[16];
            if (fs.read_s("%15s", buf, sizeof(buf))) {
                SpriteData newData;
                if (strcmp(buf, "con_wall") == 0) {
                    newData.method = TileTextureMethod::CONNECTED_WALL;
                   
                } else if (strcmp(buf, "con") == 0) {
                    newData.method = TileTextureMethod::CONNECTED;
                }
                else {
                    return false;
                }
                std::string textureName = filePath.getLeaf();
                textureName.resize(textureName.size() - 4); // Chop of .png
                newData.texture = texture.id;
                newData.dims = ui8v2(1, 1);
                auto it = mSprites.find(textureName);
                if (it != mSprites.end()) {
                    assert(false); // Sprite name conflict! Mod conflict?
                    return false;
                }
                mSprites.insert(std::make_pair(textureName, std::move(newData)));
                return true;
            } else {
                assert(false); // Bad meta file
                return false;
            }
        }

        char textureName[128];
        i32v2 tilePos;
        i32v2 tileDims;
        while (fs.read_s("%127s %d,%d %d,%d", textureName, sizeof(textureName), &tilePos.x, &tilePos.y, &tileDims.x, &tileDims.y) == 5) {
            SpriteData newData;
            f32v2 uvMult(1.0f / fileDims.x, 1.0f / fileDims.y);
            newData.texture = texture.id;
            newData.uvs.x = tilePos.x * uvMult.x;
            newData.uvs.y = (tilePos.y + 1) * uvMult.y;
            newData.uvs.z = uvMult.x;
            newData.uvs.w = -uvMult.y;
            newData.dims.x = (ui8)tileDims.x;
            newData.dims.y = (ui8)tileDims.y;

            // TODO: HashedString
            auto it = mSprites.find(textureName);
            if (it != mSprites.end()) {
                assert(false); // Sprite name conflict! Mod conflict?
                return false;
            }
            mSprites.insert(std::make_pair(textureName, std::move(newData)));
        }
    }
    else {
        // Default sprite behavior? No meta file, need a SpriteData?
    }
    return true;
}

bool ResourceManager::loadTiles(const vio::Path& filePath) {
    // Read file
    nString data;
    mIoManager->readFileToString(filePath, data);
    if (data.empty()) return false;

    // Convert to YAML
    keg::ReadContext context;
    context.env = keg::getGlobalEnvironment();
    context.reader.init(data.c_str());
    keg::Node node = context.reader.getFirst();
    if (keg::getType(node) != keg::NodeType::MAP) {
        context.reader.dispose();
        return false;
    }

    auto f = makeFunctor([&](Sender, const nString& key, keg::Node value) {
        TileData tile;

        // Load data
        keg::parse((ui8*)&tile, value, context, &KEG_GLOBAL_TYPE(TileData));

        // TODO: Serialize the string > ID mapping
        TileID nextId = ++mIdGenerator;
        assert(nextId < 0xffff); // Make sure we dont roll over
        assert(TileRepository::sTileIdMapping.find(key) == TileRepository::sTileIdMapping.end()); // Duplicate name
        // TODO: error handling  for missing  sprite
        tile.spriteData = getSprite(tile.textureName);
        assert(tile.spriteData.isValid()); // TODO: Error msg
        TileRepository::sTileIdMapping[key] = nextId;
        TileRepository::sTileData[nextId] = std::move(tile);
    });
    context.reader.forAllInMap(node, &f);
    context.reader.dispose();

    return true;
}
