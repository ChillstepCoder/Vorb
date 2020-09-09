#include "stdafx.h"
#include "ResourceManager.h"

#include "rendering/TextureAtlas.h"
#include "rendering/TileSpriteLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/IO.h>

ResourceManager::ResourceManager() {
    mTextureCache = std::make_unique<vg::TextureCache>();
    mIoManager = std::make_unique<vio::IOManager>();
    mTextureCache->init(mIoManager.get());
    mTileSpriteLoader = std::make_unique<TileSpriteLoader>(*this);
    mTextureAtlas = std::make_unique<TextureAtlas>();
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
            mTileSpriteLoader->loadSpriteSheet(entry);
        }
        else if (fileHasExtension(entry, ".tile")) {
            loadTiles(entry);
        }
    }

    std::cout << "Sprites loaded!\n";
    for (auto&& it : mSprites) {
        std::cout << "   " << it.first << " " << it.second.texture << std::endl;
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
