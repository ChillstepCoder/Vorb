#include "stdafx.h"
#include "TextureRepository.h"

#include <Vorb/graphics/TextureCache.h>

#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

TextureRepository::TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager) : mTextureCache(textureCache), mIoManager(ioManager) {

}

TextureRepository::~TextureRepository()
{

}

bool TextureRepository::loadTexture(const vio::Path& filePath) {
    std::string textureName = vio::getLeafNameFromFilePathNoExtension(filePath);
    SubTexture& newTexture = mSubTextures.emplace_back();
    newTexture.mId = mSubTextures.size() - 1;
    newTexture.mUvRect = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
    // TODO: Sampler params
    newTexture.mTextureDiffuse = mTextureCache.addTexture(filePath, textureName).id;
    assert(mTextureIdLookup.find(textureName) == mTextureIdLookup.end() && "Duplicate texture name detected");
    mTextureIdLookup[textureName] = newTexture.mId;

    newTexture.mTextureHandleDiffuse = glGetTextureHandleARB(newTexture.mTextureDiffuse);
    // Residency means it is now fixed in active memory
    glMakeTextureHandleResidentARB(newTexture.mTextureHandleDiffuse);

    checkGlError("TextureRepository::loadTexture");

    // TODO: GENERATE OR LOAD NORMALS
    newTexture.mTextureNormal = newTexture.mTextureDiffuse;
    newTexture.mTextureHandleNormal = newTexture.mTextureHandleDiffuse;

    return true;
}

SubTexture& TextureRepository::getTexture(const char* name) {
    auto&& it = mTextureIdLookup.find(name);
    assert(it != mTextureIdLookup.end());
    return mSubTextures[it->second];
}
