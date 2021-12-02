#pragma once


// Must be power of 2 and >= 16
constexpr unsigned TEXTURE_ATLAS_WIDTH_PX = 4096;
constexpr unsigned TEXTURE_ATLAS_SIZE_PX = TEXTURE_ATLAS_WIDTH_PX * TEXTURE_ATLAS_WIDTH_PX;
constexpr unsigned TEXTURE_ATLAS_CELL_WIDTH_PX = 128;
constexpr unsigned TEXTURE_ATLAS_CELLS_PER_ROW = TEXTURE_ATLAS_WIDTH_PX / TEXTURE_ATLAS_CELL_WIDTH_PX;
constexpr unsigned TEXTURE_ATLAS_CELLS_PER_PAGE = TEXTURE_ATLAS_CELLS_PER_ROW * TEXTURE_ATLAS_CELLS_PER_ROW;
constexpr unsigned TEXTURE_ATLAS_MAX_DEPTH = GL_MAX_ARRAY_TEXTURE_LAYERS;
// Layer 1 = color, layer 2 = normals
constexpr int TEXTURE_ATLAS_LAYERS_PER_PAGE = 2;

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
    f32v4 writePixels(ui32 cellIndex, ui32 cellsX, ui32 cellsY, const color4* srcPixels, ui32 srcResourceWidthPx, ui32 srcRectWidthPx, ui32 srcRectHeightPx); 
    void uploadDirtyPages();
    void generateMipMaps() const;
    void compressTextures() const;

    void writeDebugPages();

    VGTexture getAtlasTexture() const { return mAtlasTexture; }
    unsigned getPageIndexFromCellIndex(unsigned cellIndex);
    unsigned getNumPages() const { return (unsigned)mPages.size(); }

private:
    void addPage();
    void uploadPage(AtlasPage& page);
    ui32v2 getPageCoordsFromCellIndex(unsigned cellIndex);
    void allocateTexture(ui32 texture, int internalFormat) const;

    std::vector<AtlasPage> mPages;
    mutable VGTexture mAtlasTexture;
    mutable bool mNeedsReallocate;
};

