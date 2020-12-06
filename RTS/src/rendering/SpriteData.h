#pragma once

#include <Vorb/graphics/Texture.h>

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    COUNT
};
KEG_ENUM_DECL(TileTextureMethod);

constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 4;

enum SpriteDataFlags : ui8 {
    SPRITEDATA_HAS_NORMAL_MAP = 1 << 0
};

struct SpriteData {

    const bool isValid() const { return texture != 0; }

    f32v4 uvs = f32v4(0, 0, 1, 1);
    f32v2 dimsMeters = f32v2(1, 1);
    VGTexture texture = 0;
    TileTextureMethod method = TileTextureMethod::SIMPLE;
    ui16 atlasPage = 0;
    color3 lodColor = color3(255, 0, 255);
    ui8 flags = 0;
};
KEG_TYPE_DECL(TileData);