#pragma once

#include <Vorb/graphics/SamplerState.h>

typedef ui32 CubemapID;

DECL_VG(class ScopedBitmapResource);

struct CubemapFileData {
    nString mTexPosX;
    nString mTexNegX;
    nString mTexPosY;
    nString mTexNegY;
    nString mTexPosZ;
    nString mTexNegZ;
    vg::SamplerStateType mSamplerState = vg::SamplerStateType::LINEAR_CLAMP_MIPMAP;
};
KEG_TYPE_DECL(CubemapFileData);

class Cubemap
{
public:
    VORB_NON_COPYABLE(Cubemap);
    Cubemap(CubemapID id);
    ~Cubemap();

    // Must be called sequencailly 0-6
    bool initFace(int face, const vg::ScopedBitmapResource& rs);

    VGTexture getTexture() const { return mTexture; }
    VGTexture getIrradianceTexture() const { return mIrradianceMap; }
    VGTexture getPrefilterMap() const { return mPrefilterMap; }
    void computePBRMaps();
private:
    void createMipmaps();
    void computeIrradianceMap();
    void computePrefilterMap();
    CubemapID mId;
    VGTexture mTexture = 0;
    VGTexture mIrradianceMap = 0;
    VGTexture mPrefilterMap = 0;
    ui32v2 mDims;
};

