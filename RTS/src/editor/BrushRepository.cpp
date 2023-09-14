#include "stdafx.h"
#include "BrushRepository.h"

#include "resources/TextureRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/io/FileOps.h>

#include <gli/texture2d.hpp>

#include "io/PngLoader.h"

AssetLoadFunc BrushRepository::getAssetLoadFunc() {

    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        BrushDef& def = *static_cast<BrushDef*>(assetDataPtr);
        
        TextureRepository& textureRepository = TextureRepository::get();

        fs::path stdPath(filePath.getString());
        def.rs = std::make_unique<gli::texture2d>(PngLoader::loadPng(stdPath, false /*flipV*/));

        def.dims = ui32v2(def.rs->extent().x, def.rs->extent().y);

        assert(def.rs->format() == gli::FORMAT_RGBA8_UNORM_PACK8 || def.rs->format() == gli::FORMAT_R8_UNORM_PACK8);
        const int numChannels = gli::component_count(def.rs->format());
        const int byteDepth = gli::block_size(def.rs->format()) / numChannels;
        if (byteDepth != 1) {
            LOG_CRITICAL("Unimplemented byte depth {} in convertToR8", byteDepth);
            throw std::exception("Invalid block size in convertToR8");
        }
        def.data.resize(def.rs->extent().x * def.rs->extent().y);
        // Copy alpha channel only
        const ui8* rsData = def.rs->data<ui8>();
        for (std::size_t i = 0; i < def.data.size(); ++i) {
            const ui8* pixelData = rsData + i * numChannels;
            def.data[i] = pixelData[numChannels - 1];  // Alpha is the last byte in the pixel data
        }
    };
}


AssetLoadFunc BrushRepository::getAssetLoadRenderProcessFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        BrushDef& def = *static_cast<BrushDef*>(assetDataPtr);
        const GLTexture& texture = TextureRepository::get().uploadTexture(*(def.rs), vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP);
        VGTexture textureHandle = texture.getHandle();
        if (textureHandle) {
            def.texture = textureHandle;
        }
        else {
            panic("Failed to load texture {} for brush", filePath.getString().c_str());
        }

        def.rs.reset();
    };
}
