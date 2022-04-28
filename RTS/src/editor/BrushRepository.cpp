#include "stdafx.h"
#include "BrushRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/io/FileOps.h>

BrushRepository::BrushRepository(vio::IOManager& ioManager) : mIomanager(ioManager) {

}

BrushRepository::~BrushRepository() {
    for (auto&& brush : mBrushes) {
        delete[] brush.data;
    }
}

void BrushRepository::loadBrush(const vio::Path& filePath, vg::TextureCache& textureCache) {

    Brush brush;
    nString leafName = vio::getLeafNameFromFilePathNoExtension(filePath);
    vg::ScopedBitmapResource rs;
    vg::Texture texture = textureCache.addTexture(
        filePath,
        leafName,
        rs,
        vg::ImageIOFormat::RGBA_UI8,
        vg::TextureTarget::TEXTURE_2D,
        &vg::sSamplerStates.LINEAR_WRAP,
        vg::TextureInternalFormat::COMPRESSED_RGBA
    );

    if (texture.id) {
        brush.name = std::move(leafName);
        brush.texture = texture.id;
        brush.dims = ui32v2(texture.width, texture.height);

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
