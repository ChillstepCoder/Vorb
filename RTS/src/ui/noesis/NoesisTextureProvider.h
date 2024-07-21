#pragma once

#include <NsCore/Noesis.h>
#include <NsGui/TextureProvider.h>
#include <NsApp/ProvidersApi.h>

class TextureDef;

class NoesisTextureProvider : public Noesis::TextureProvider {
public:
    /// Returns metadata for the texture at the given URI or empty rectangle if texture is not found
    Noesis::TextureInfo GetTextureInfo(const Noesis::Uri& uri);

    /// Returns a texture compatible with the given device or null if texture is not found
    Noesis::Ptr<Noesis::Texture> LoadTexture(const Noesis::Uri& uri, Noesis::RenderDevice* device);
private:
    const TextureDef& getTextureDef(const Noesis::Uri& uri);
};

