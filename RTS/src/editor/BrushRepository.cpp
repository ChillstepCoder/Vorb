#include "stdafx.h"
#include "BrushRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/io/FileOps.h>
#include <Vorb/graphics/ImageIO.h>

#include <gli/texture2d.hpp>

BrushRepository::BrushRepository(vio::IOManager& ioManager) : mIomanager(ioManager) {

}

BrushRepository::~BrushRepository() {

}

void BrushRepository::loadBrush(const vio::Path& filePath, TextureRepository& textureRepository) {

    Brush brush;
    nString leafName = vio::getLeafNameFromFilePathNoExtension(filePath);
    gli::texture2d rs;
    const TextureData* textureData = textureRepository.loadTexture(filePath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_WRAP, vg::TextureInternalFormat::RGBA8, false, &rs);
    assert(textureData);

    const GLTexture& texture = textureData->texture;
    VGTexture textureHandle = texture.getHandle();
    if (textureHandle) {
        brush.name = std::move(leafName);
        brush.texture = textureHandle;
        brush.dims = texture.getDims();

        // Copy alpha channel only
        // TODO: Grayscale brushes!!!
        assert(rs.format() == gli::FORMAT_RGBA8_UNORM_PACK8);
        brush.data.resize(rs.extent().x * rs.extent().y * 4);
        // Not using rs.size() since it includes mip data
        for (std::size_t i = 0; i < rs.extent().x * rs.extent().y * 4; i += 4) {
            ui8* pixelData = static_cast<ui8*>(rs.data()) + i;
            brush.data[i] = pixelData[3];  // Alpha is the 4th byte in the pixel data
        }

        mBrushes.emplace_back(std::move(brush));
    }
    else {
        pError("Failed to load texture " + filePath.getString() + " for brush.");
        assert(false);
    }
}
