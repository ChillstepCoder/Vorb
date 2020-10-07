#pragma once

#include <Vorb/graphics/Texture.h>

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL
};

struct SpriteData {

    const bool isValid() const { return texture != 0; }

    f32v4 uvs = f32v4(0, 0, 1, 1);
    f32v2 dimsMeters = f32v2(1, 1);
    VGTexture texture = 0;
    TileTextureMethod method = TileTextureMethod::SIMPLE;
    ui16 atlasPage = 0;
};