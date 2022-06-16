#pragma once

#include "rendering/texture/SubTexture.h"

// MUST BE POWER OF 2 >= 16
const ui32 FONT_PX_SIZE = 32;

struct CharGlyph {
public:
    char character;
    f32v4 uvRect;
    f32v2 size;
};

enum class TextAlign {
    NONE,
    LEFT,
    TOP_LEFT,
    TOP,
    TOP_RIGHT,
    RIGHT,
    BOTTOM_RIGHT,
    BOTTOM,
    BOTTOM_LEFT,
    CENTER,
};

struct Font {
    std::vector<CharGlyph> mGlyphs;
    ui32 mFontHeight = 0;
    SubTexture mTexture;
};
