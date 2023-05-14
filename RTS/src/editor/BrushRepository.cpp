#include "stdafx.h"
#include "BrushRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/io/FileOps.h>

#include <gli/texture2d.hpp>

BrushRepository::BrushRepository(vio::IOManager& ioManager) : mIomanager(ioManager) {

}

BrushRepository::~BrushRepository() {

}

void BrushRepository::loadBrush(const vio::Path& filePath, TextureRepository& textureRepository) {

    Brush brush;
    nString leafName = vio::getLeafNameFromFilePathNoExtension(filePath);
    gli::texture2d rs;
    const TextureData* textureData = textureRepository.loadTexture(filePath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_WRAP, false, &rs);
    assert(textureData);

    const GLTexture& texture = textureData->texture;
    VGTexture textureHandle = texture.getHandle();
    if (textureHandle) {
        brush.name = std::move(leafName);
        brush.texture = textureHandle;
        brush.dims = texture.getDims();

        assert(rs.format() == gli::FORMAT_RGBA8_UNORM_PACK8 || rs.format() == gli::FORMAT_R8_UNORM_PACK8);
        const int numChannels = gli::component_count(rs.format());
        const int byteDepth = gli::block_size(rs.format()) / numChannels;
        if (byteDepth != 1) {
            LOG_CRITICAL("Unimplemented byte depth {} in convertToR8", byteDepth);
            throw std::exception("Invalid block size in convertToR8");
        }
        brush.data.resize(rs.extent().x * rs.extent().y);
        // Copy alpha channel only
        for (std::size_t i = 0; i < brush.data.size(); ++i) {
            const ui8* pixelData = rs.data<ui8>() + i * numChannels;
            brush.data[i] = pixelData[numChannels - 1];  // Alpha is the last byte in the pixel data
        }

        mBrushes.emplace_back(std::move(brush));
    }
    else {
        pError("Failed to load texture " + filePath.getString() + " for brush.");
        assert(false);
    }
}
