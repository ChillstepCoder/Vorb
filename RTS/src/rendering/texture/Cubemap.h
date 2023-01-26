#pragma once

#include <Vorb/graphics/SamplerState.h>

typedef ui32 CubemapID;

struct CubemapFileData {
    nString mTexPosX;
    nString mTexNegX;
    nString mTexPosY;
    nString mTexNegY;
    nString mTexPosZ;
    nString mTexNegZ;
    vg::SamplerStateType mSamplerState = vg::SamplerStateType::LINEAR_WRAP_MIPMAP;
};
KEG_TYPE_DECL(CubemapFileData);

class Cubemap
{
public:
    VORB_NON_COPYABLE(Cubemap);
    Cubemap(CubemapID id);
    ~Cubemap();

    VGTexture getTexture() const { return mTexture; }
private:
    CubemapID mId;
    VGTexture mTexture = 0;
};

