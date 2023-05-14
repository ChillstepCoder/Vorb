#pragma once

#include <gli/texture2d.hpp>

class TextureConvert {
public:
    // Does not handle mipmap, only base layer
    static gli::texture2d convertToR8(const gli::texture2d& inputTexture);
};

