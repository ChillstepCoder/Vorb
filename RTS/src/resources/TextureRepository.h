#pragma once
// NEW

#include "rendering/texture/SubTexture.h"

DECL_VG(class TextureCache);
DECL_VIO(class IOManager);


class TextureRepository {
public:
    TextureRepository(vg::TextureCache& textureCache, vio::IOManager& ioManager);
    ~TextureRepository();

    bool loadTexture(const vio::Path& filePath);
    SubTexture& getTexture(const char* name);

private:
    std::vector<SubTexture> mSubTextures;
    std::map<nString, SubTextureID> mTextureIdLookup;
    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
};
