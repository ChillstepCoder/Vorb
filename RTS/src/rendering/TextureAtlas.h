#pragma once


// Must be power of 2 and >= 16
constexpr unsigned TEXTURE_ATLAS_WIDTH_PX = 2048;
constexpr unsigned TEXTURE_ATLAS_SIZE_PX = TEXTURE_ATLAS_WIDTH_PX * TEXTURE_ATLAS_WIDTH_PX;
constexpr unsigned TEXTURE_ATLAS_CELL_WIDTH_PX = 16;
constexpr unsigned TEXTURE_ATLAS_CELLS_PER_ROW = TEXTURE_ATLAS_WIDTH_PX / TEXTURE_ATLAS_CELL_WIDTH_PX;
constexpr unsigned TEXTURE_ATLAS_CELLS_PER_PAGE = TEXTURE_ATLAS_CELLS_PER_ROW * TEXTURE_ATLAS_CELLS_PER_ROW;
constexpr unsigned TEXTURE_ATLAS_MAX_DEPTH = GL_MAX_ARRAY_TEXTURE_LAYERS;

struct AtlasPage {
    AtlasPage() : dirty(true) {}
    std::vector<color4> pixels;
    unsigned index = 0;
    bool dirty;
};
class TextureAtlas {
public:
    TextureAtlas();
    ~TextureAtlas();

    // Returns UVRect
    f32v4 writePixels(unsigned cellIndex, unsigned cellsX, unsigned cellsY, const color4* srcPixels, unsigned srcWidthPx);
    void uploadDirtyPages();

    void writeDebugPages();

    VGTexture getAtlasTexture() const { return mAtlasTexture; }
    unsigned getPageIndexFromCellIndex(unsigned cellIndex);

private:
    void addPage();
    void uploadPage(AtlasPage& page);
    ui32v2 getPageCoordsFromCellIndex(unsigned cellIndex);
    void allocateTexture();

    std::vector<AtlasPage> mPages;
    VGTexture mAtlasTexture;
    bool mNeedsReallocate;
};

