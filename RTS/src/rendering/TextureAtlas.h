#pragma once

constexpr unsigned TEXTURE_ATLAS_SIZE = 2048;
constexpr unsigned MAX_TEXTURE_ATLAS_DEPTH = GL_MAX_ARRAY_TEXTURE_LAYERS;

struct AtlasPage {
    AtlasPage() : dirty(true) {}
    std::vector<color4> pixels;
    bool dirty;
};
class TextureAtlas {
public:

    void uploadDirtyPages();

    std::vector<AtlasPage> mPages;
};

