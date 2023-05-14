#pragma once

#include <gli/texture2d.hpp>

class TextureConvert {
public:
    // Does not handle mipmap, only base layer
    static gli::texture2d convertToR8(const gli::texture2d& inputTexture);
    // convert to DDS compressed texture using optimal BCX encoding and
    // generate mipmaps. InputTexture is assumed to have no mip levels
    static gli::texture2d convertToDDS(const gli::texture2d& inputTexture);
};

