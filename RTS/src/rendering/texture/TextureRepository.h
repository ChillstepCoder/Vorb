#pragma once
// NEW

DECL_VG(class TextureCache);
DECL_VIO(class IOManager);

typedef ui32 SubTextureID;

struct SubTexture {
    SubTextureID mId;
    VGTexture mTexture;
    f32v4 mUvRect;
};

class TextureRepository {
public:
    TextureRepository(vio::IOManager& ioManager);
    ~TextureRepository();

    bool loadTexture(const vio::Path& filePath);

private:
    std::vector<SubTexture> mSubTextures;
    std::map<nString, SubTextureID> mTextureIdLookup;
    vio::IOManager& mIoManager;
};
