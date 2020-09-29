#pragma once

#include <Vorb/io/Path.h>
#include "rendering/SpriteData.h"

DECL_VG(class TextureCache);
DECL_VIO(class IOManager);

class TileSpriteLoader;
class TextureAtlas;

class SpriteRepository
{
    friend class TileSpriteLoader;
    friend class ResourceManager;
public:
    SpriteRepository(vio::IOManager& ioManager);
    ~SpriteRepository();

    const SpriteData& getSprite(const std::string& spriteName);
    bool loadSpriteTexture(const vio::Path& filePath);

    vg::TextureCache& getTextureCache() { return *mTextureCache; }
    const TextureAtlas& getTextureAtlas() { return *mTextureAtlas; }

private:
    std::unique_ptr<TileSpriteLoader> mTileSpriteLoader;
    std::unique_ptr<vg::TextureCache> mTextureCache;
    std::unique_ptr<TextureAtlas> mTextureAtlas;
    std::map<std::string /* Sprite Name */, SpriteData> mSprites;

    vio::IOManager& mIoManager;
};

