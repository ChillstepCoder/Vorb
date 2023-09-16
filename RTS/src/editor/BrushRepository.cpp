#include "stdafx.h"
#include "BrushRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/io/FileOps.h>

#include <gli/texture2d.hpp>

#include "io/PngLoader.h"

AssetLoadFunc BrushRepository::getAssetLoadFunc() {

    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {

        BrushDef& def = *static_cast<BrushDef*>(assetDataPtr);
        gli::texture2d& rs = std::any_cast<gli::texture2d&>(userData);

        TextureRepository& textureRepository = TextureRepository::get();

        fs::path stdPath(filePath.getString());
        rs = PngLoader::loadPng(stdPath, false /*flipV*/);

        def.dims = ui32v2(rs.extent().x, rs.extent().y);

        assert(rs.format() == gli::FORMAT_RGBA8_UNORM_PACK8 || rs.format() == gli::FORMAT_R8_UNORM_PACK8);
        const int numChannels = gli::component_count(rs.format());
        const int byteDepth = gli::block_size(rs.format()) / numChannels;
        if (byteDepth != 1) {
            LOG_CRITICAL("Unimplemented byte depth {} in convertToR8", byteDepth);
            throw std::exception("Invalid block size in convertToR8");
        }
        def.data.resize(rs.extent().x * rs.extent().y);
        // Copy alpha channel only
        const ui8* rsData = rs.data<ui8>();
        for (std::size_t i = 0; i < def.data.size(); ++i) {
            const ui8* pixelData = rsData + i * numChannels;
            def.data[i] = pixelData[numChannels - 1];  // Alpha is the last byte in the pixel data
        }
    };
}


AssetLoadFunc BrushRepository::getAssetLoadRenderProcessFunc() {

    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {

        gli::texture2d& rs = std::any_cast<gli::texture2d&>(userData);
        BrushDef& def = *static_cast<BrushDef*>(assetDataPtr);
        const GLTexture& texture = TextureRepository::get().uploadTexture(rs, vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP);
        VGTexture textureHandle = texture.getHandle();
        if (textureHandle) {
            def.texture = textureHandle;
        }
        else {
            panic("Failed to load texture {} for brush", filePath.getString().c_str());
        }
    };
}

std::any BrushRepository::getUserData(AssetID id) {
    return gli::texture2d();
}