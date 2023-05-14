#pragma once

#include <Vorb/graphics/SamplerState.h>
#include <gli/texture2d.hpp>

typedef ui32 CubemapID;

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
    bool initFace(int face, const gli::texture2d& rs);

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

