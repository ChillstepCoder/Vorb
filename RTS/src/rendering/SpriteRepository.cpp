#include "stdafx.h"
#include "SpriteRepository.h"

#include <Vorb/graphics/TextureCache.h>
#include "TileSpriteLoader.h"
#include "TextureAtlas.h"

SpriteData DEFAULT_SPRITE_DATA;

SpriteRepository::SpriteRepository(vio::IOManager& ioManager) :
    mIoManager(ioManager)
{
    mTextureCache = std::make_unique<vg::TextureCache>();
    mTextureCache->init(&mIoManager);
    mTextureAtlas = std::make_unique<TextureAtlas>();
    mTileSpriteLoader = std::make_unique<TileSpriteLoader>(*this, *mTextureAtlas, mIoManager);
}

SpriteRepository::~SpriteRepository()
{

}

const SpriteData& SpriteRepository::getSprite(const std::string& spriteName) {

    auto it = mSprites.find(spriteName);
    if (it != mSprites.end()) {
        return it->second;
    }
    return DEFAULT_SPRITE_DATA;

}

bool SpriteRepository::loadSpriteTexture(const vio::Path& filePath) {

    return mTileSpriteLoader->loadSpriteTexture(filePath);

}
