#include "stdafx.h"
#include "NoesisTextureProvider.h"

#include <NsCore/String.h>
#include <NsCore/StringUtils.h>
#include <NsGui/Uri.h>
#include <NsRender/Texture.h>

#include "ui/noesis/NoesisGLRenderDevice.h"

#include "definitions/rendering/TextureDef.h"
#include "resources/TextureRepository.h"

Noesis::TextureInfo NoesisTextureProvider::GetTextureInfo(const Noesis::Uri& uri) {
    const TextureDef& texture = getTextureDef(uri);

    Noesis::TextureInfo info;
    info.dpiScale = 1.0f;
    info.width = texture.gpuTexture.getDims().x;
    info.height = texture.gpuTexture.getDims().y;
    info.x = 0;
    info.y = 0;

    return info;
}

Noesis::Ptr<Noesis::Texture> NoesisTextureProvider::LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device) {
    // Premultiplied textures only
    const TextureDef& texture = getTextureDef(uri);

    NoesisGLRenderDevice& glDevice = *static_cast<NoesisGLRenderDevice*>(device);

    bool hasAlpha;
    switch (texture.gpuTexture.getFormat()) {
        case vg::TextureFormat::ALPHA:
        case vg::TextureFormat::ALPHA_INTEGER:
        case vg::TextureFormat::BGRA:
        case vg::TextureFormat::BGRA_INTEGER:
        case vg::TextureFormat::LUMINANCE_ALPHA:
        case vg::TextureFormat::RGBA:
        case vg::TextureFormat::RGBA_INTEGER:
            hasAlpha = true;
            break;
        default:
            hasAlpha = false;
            break;
    }

    return glDevice.WrapTexture(
        texture.gpuTexture.getHandle(),
        texture.gpuTexture.getDims().x,
        texture.gpuTexture.getDims().y,
        texture.gpuTexture.getMipLevels(),
        true /*inverted*/,
        hasAlpha
    );
}

const TextureDef& NoesisTextureProvider::getTextureDef(const Noesis::Uri& uri) {
    Noesis::FixedString<512> str;
    uri.GetPath(str);
    assert(Noesis::StrEndsWith(str.Begin(), ".png"));
    // Only grap the leaf
    str[str.Size() - 4] = '\0'; // Remove .png
    const int lastSlash = Noesis::StrFindLast(str.Begin(), "/");
    const int begin = (lastSlash != -1) ? (lastSlash + 1) : 0;

    const StrToken token(str.Begin() + begin);
    TextureRepository& repo = TextureRepository::get();
    const AssetID id = repo.getAssetID(token);
    assert(repo.isAssetLoaded(id));
    return repo.getLoadedAsset(id);
}
