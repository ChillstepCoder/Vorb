#pragma once

#include <Vorb/graphics/Texture.h>

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    VERTICAL,
    FLORA,
    WORLD_TILING,
    COUNT
};
KEG_ENUM_DECL(TileTextureMethod);

constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 5;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_HEIGHT = 3;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_WIDTH = 1;

enum SpriteDataFlags : ui8 {
    SPRITEDATA_FLAG_HAS_NORMAL_MAP = 1 << 0,
    SPRITEDATA_FLAG_RAND_FLIP      = 1 << 1,
    SPRITEDATA_FLAG_TRANSPARENT    = 1 << 2
};

struct SpriteData {

    const bool isValid() const { return texture != 0; }

    f32v4 uvs = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
    f32v2 dimsMeters = f32v2(1.0f, 1.0f);
    f32v2 offset = f32v2(0.0f);
    ui32v2 variantCount = ui32v2(1);
    ui32v2 bunchCount = ui32v2(1);
    f32v2 sizeRange = f32v2(1.0f);
    VGTexture texture = 0;
    TileTextureMethod method = TileTextureMethod::SIMPLE;
    ui16 atlasPage = 0;
    ui8 flags = 0;
};