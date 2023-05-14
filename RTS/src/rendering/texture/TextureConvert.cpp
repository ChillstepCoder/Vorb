#include "stdafx.h"
#include "TextureConvert.h"

gli::texture2d TextureConvert::convertToR8(const gli::texture2d& inputTexture) {
    constexpr int MAX_LEVEL = 1;
    const int numChannels = gli::component_count(inputTexture.format());
    const int byteDepth = gli::block_size(inputTexture.format()) / numChannels;
    if (byteDepth != 1) {
        LOG_CRITICAL("Unimplemented byte depth {} in convertToR8", byteDepth);
        throw std::exception("Invalid block size in convertToR8");
    }
    if (numChannels == 1) {
        LOG_CRITICAL("Tried to convert a texture that was already r8");
        throw std::exception("Tried to convert a texture that was already r8");
    }
    gli::texture2d newTexture(gli::FORMAT_R8_UNORM_PACK8, inputTexture.extent(), MAX_LEVEL);
    for (std::size_t i = 0; i < newTexture.size(0); ++i) {
        const ui8* pixelData = inputTexture.data<ui8>() + i * numChannels;
        newTexture.data<ui8>()[i] = pixelData[0];  // Grab R channel
    }
    return newTexture;
}
