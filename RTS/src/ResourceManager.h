#pragma once

DECL_VG(class TextureCache);
DECL_VIO(class IOManager);

#include <Vorb/io/Path.h>
#include "rendering/SpriteData.h"
#include "world/Tile.h"

class TileSpriteLoader;
class TextureAtlas;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
    friend class TileSpriteLoader;
public:
    ResourceManager();
    ~ResourceManager();

    void loadResources(const vio::Path& folderPath);

    // Resource Accessors
    SpriteData getSprite(const std::string& spriteName);

    TextureAtlas& getTextureAtlas() { return *mTextureAtlas; }
    vg::TextureCache& getTextureCache() { return *mTextureCache.get(); }
    bool hasLoadedResources() const { return mHasLoadedResources; }
    
private:
    bool loadTiles(const vio::Path& filePath);

    // Textures
    std::unique_ptr<TileSpriteLoader> mTileSpriteLoader;
    std::unique_ptr<vg::TextureCache> mTextureCache;
    std::unique_ptr<TextureAtlas> mTextureAtlas;
    std::map<std::string /* Sprite Name */, SpriteData> mSprites;

    std::unique_ptr<vio::IOManager> mIoManager;

    TileID mIdGenerator = 0; // Generates IDs

    bool mHasLoadedResources = false;
};

