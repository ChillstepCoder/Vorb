#pragma once

#include <Vorb/graphics/SamplerState.h>
#include <gli/gli.hpp>
#include <gli/texture.hpp>

class CubemapDef : public IAsset
{
public:
    friend class CubemapRepository;

    DEFAULT_ASSET_CONSTRUCTOR(CubemapDef);
    VORB_NON_COPYABLE(CubemapDef);
    ~CubemapDef();


    VGTexture getTexture() const { return mTexture; }
    VGTexture getIrradianceTexture() const { return mIrradianceMap; }
    VGTexture getPrefilterMap() const { return mPrefilterMap; }


private:
    VGTexture mTexture = 0;
    VGTexture mIrradianceMap = 0;
    VGTexture mPrefilterMap = 0;
    ui32v2 mDims;
};

