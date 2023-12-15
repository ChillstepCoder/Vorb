#include "stdafx.h"
#include "ModelBillboardLodManager.h"

#include "resources/TextureRepository.h"
#include "resources/ModelRepository.h"

ModelBillboardLodManager::ModelBillboardLodManager(const std::unordered_map<AssetID, GLTexture>& billboardTextures) :
    mBillboardTextures(billboardTextures) {

}

ModelBillboardLodManager::~ModelBillboardLodManager() = default;

void ModelBillboardLodBuilder::initTextureForModel(AssetID modelID) {
    assert(!mBillboardTextures.contains(modelID));

    // TODO: DXT Compressed as well
    ui32v2 dims(128, 256);
    gli::extent2d extent(dims.x, dims.y);
    gli::texture2d texture(gli::FORMAT_RGBA8_UNORM_PACK8, extent);

    color4 pixelColor = color::Red;
    for (int i = 0; i < texture.size(); ++i) {
        ((color4*)texture.data())[i] = pixelColor;
    }

    // TODO: Create mips?
    mBillboardTextures.emplace(modelID, TextureRepository::get().uploadTexture(texture, vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP_MIPMAP));
}
