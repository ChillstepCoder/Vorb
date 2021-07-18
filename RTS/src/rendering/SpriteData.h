#pragma once

#include <Vorb/graphics/Texture.h>

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    FLORA,
    COUNT
};
KEG_ENUM_DECL(TileTextureMethod);

constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 5;

enum SpriteDataFlags : ui8 {
    SPRITEDATA_FLAG_HAS_NORMAL_MAP = 1 << 0,
    SPRITEDATA_FLAG_RAND_FLIP      = 1 << 1,
    SPRITEDATA_FLAG_OPAQUE         = 1 << 2,
    SPRITEDATA_FLAG_RENDER_LOD     = 1 << 3,
};

const color3 NO_LOD_COLOR = color3(255, 0, 255);

struct SpriteData {

    const bool isValid() const { return texture != 0; }

    f32v4 uvs = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
    f32v2 dimsMeters = f32v2(1.0f, 1.0f);
    f32v2 offset = f32v2(0.0f);
    VGTexture texture = 0;
    TileTextureMethod method = TileTextureMethod::SIMPLE;
    ui16 atlasPage = 0;
    color3 lodColor = NO_LOD_COLOR;
    ui8 flags = 0;
};