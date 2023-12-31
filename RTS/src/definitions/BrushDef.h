#pragma once

#include "rendering/texture/GLTexture.h"

class BrushDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BrushDef, AssetType::Brush);

    std::vector<ui8> data; // A8 alpha only
    ui32v2 dims;
    std::unique_ptr<GLTexture> texture;
};