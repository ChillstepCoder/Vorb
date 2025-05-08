#pragma once

#include "rendering/texture/GLTexture.h"

#include <Vorb/graphics/SamplerState.h>
#include <gli/gli.hpp>
#include <gli/texture.hpp>

class TextureDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TextureDef, AssetType::Texture);

    VGTexture getTextureHandle() const { return gpuTexture.getHandle(); }

    GLTexture gpuTexture;
    const vg::SamplerState* samplerState = nullptr;
    bool flipV = false; // TODO: Flags

};
