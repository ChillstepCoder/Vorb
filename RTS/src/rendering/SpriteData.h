#pragma once

#include <Vorb/graphics/Texture.h>

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED
};

struct SpriteData {

    const bool isValid() const { return texture != 0; }

    // TODO: Compress UVs into 8 bit
    f32v4 uvs;
    VGTexture texture = 0;
    ui8v2 dims;
    TileTextureMethod method = TileTextureMethod::SIMPLE;
};