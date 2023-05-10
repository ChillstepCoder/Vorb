#include "stdafx.h"
#include "BrushRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/io/FileOps.h>
#include <Vorb/graphics/ImageIO.h>

BrushRepository::BrushRepository(vio::IOManager& ioManager) : mIomanager(ioManager) {

}

BrushRepository::~BrushRepository() {
    for (auto&& brush : mBrushes) {
        delete[] brush.data;
    }
}

void BrushRepository::loadBrush(const vio::Path& filePath, TextureRepository& textureRepository) {

    Brush brush;
    nString leafName = vio::getLeafNameFromFilePathNoExtension(filePath);
    vg::ScopedBitmapResource rs;
    const TextureData* data = textureRepository.loadTexture(filePath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_WRAP, vg::TextureInternalFormat::RGBA8, false, &rs);
    assert(data);

    const GLTexture& texture = data->texture;
    VGTexture textureHandle = texture.getHandle();
    if (textureHandle) {
        brush.name = std::move(leafName);
        brush.texture = textureHandle;
        brush.dims = texture.getDims();

        // Copy alpha channel only
        brush.data = new ui8[rs.height * rs.width];
        for (ui32 y = 0; y < rs.height; ++y) {
            for (ui32 x = 0; x < rs.width; ++x) {
                ui32 pixel = y * rs.width + x;
                brush.data[pixel] = rs.bytesUI8v4[pixel].a;
            }
        }

        mBrushes.emplace_back(std::move(brush));
    }
    else {
        pError("Failed to load texture " + filePath.getString() + " for brush.");
        assert(false);
    }
}
