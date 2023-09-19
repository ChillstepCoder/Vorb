#pragma once

#include "rendering/texture/GLTexture.h"

#include <Vorb/graphics/SamplerState.h>
#include <gli/gli.hpp>
#include <gli/texture.hpp>

class TextureDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TextureDef);

    GLTexture gpuTexture;
    vg::TextureTarget type;
    const vg::SamplerState* samplerState;
    bool flipV; // TODO: Flags

};
