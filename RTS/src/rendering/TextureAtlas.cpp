#include "stdafx.h"
#include "TextureAtlas.h"

#include <SDL.h>

TextureAtlas::TextureAtlas()
{
    glGenTextures(1, &mAtlasTexture);

    addPage();
}

TextureAtlas::~TextureAtlas()
{
    glDeleteTextures(1, &mAtlasTexture);
}

f32v4 TextureAtlas::writePixels(unsigned cellIndex, unsigned cellsX, unsigned cellsY, const color4* srcPixels, unsigned srcWidthPx) {
    f32v4 uvRect;
    const ui32v2 coords = getPageCoordsFromCellIndex(cellIndex);
    const unsigned pageIndex = getPageIndexFromCellIndex(cellIndex);
    // Expand to needed pages
    while (pageIndex >= mPages.size()) {
        addPage();
    }

    assert(cellsX < TEXTURE_ATLAS_CELLS_PER_ROW && cellsY < TEXTURE_ATLAS_CELLS_PER_ROW);

    auto& dstPixels = mPages[pageIndex].pixels;

    // Copy each row to the atlas
    for (unsigned pxY = 0; pxY < TEXTURE_ATLAS_CELL_WIDTH_PX * cellsY; ++pxY) {
        const unsigned dstY = (coords.y * TEXTURE_ATLAS_CELL_WIDTH_PX + pxY);
        memcpy(
            &dstPixels[dstY * TEXTURE_ATLAS_WIDTH_PX + (unsigned)coords.x * TEXTURE_ATLAS_CELL_WIDTH_PX],
            &srcPixels[pxY * srcWidthPx],
            sizeof(color4) * TEXTURE_ATLAS_CELL_WIDTH_PX * cellsX
        );
    }

    uvRect.x = (float)coords.x / TEXTURE_ATLAS_CELLS_PER_ROW;
    uvRect.y = (float)coords.y / TEXTURE_ATLAS_CELLS_PER_ROW;
    uvRect.z = (float)cellsX / TEXTURE_ATLAS_CELLS_PER_ROW;
    uvRect.w = (float)cellsY / TEXTURE_ATLAS_CELLS_PER_ROW;

    mPages[pageIndex].dirty = true;
    return uvRect;
}

void TextureAtlas::uploadDirtyPages()
{
    if (mNeedsReallocate) {
        allocateTexture();
        // Upload all pages
        glBindTexture(GL_TEXTURE_2D_ARRAY, mAtlasTexture);
        for (size_t i = 0; i < mPages.size(); i++) {
            uploadPage(mPages[i]);
        }
    }
    else {
        // Upload dirty pages
        glBindTexture(GL_TEXTURE_2D_ARRAY, mAtlasTexture);
        for (size_t i = 0; i < mPages.size(); i++) {
            if (mPages[i].dirty) {
                uploadPage(mPages[i]);
            }
        }
    }
}

void TextureAtlas::addPage() {
    mPages.emplace_back();
    auto& page = mPages.back();
    page.pixels.resize(TEXTURE_ATLAS_SIZE_PX, color4(255, 0, 255, 255));
    page.index = mPages.size() - 1;
    mNeedsReallocate = true;
}

void TextureAtlas::uploadPage(AtlasPage& page) {
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, page.index, TEXTURE_ATLAS_WIDTH_PX, TEXTURE_ATLAS_WIDTH_PX, 1, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels.data());
    page.dirty = false;
}

unsigned TextureAtlas::getPageIndexFromCellIndex(unsigned cellIndex) {
    return cellIndex / TEXTURE_ATLAS_CELLS_PER_PAGE;
}

ui32v2 TextureAtlas::getPageCoordsFromCellIndex(unsigned cellIndex) {
    ui32v2 rv;
    rv.x = cellIndex % TEXTURE_ATLAS_CELLS_PER_ROW;
    rv.y = (cellIndex % TEXTURE_ATLAS_CELLS_PER_PAGE) / TEXTURE_ATLAS_CELLS_PER_ROW;
    return rv;
}

void TextureAtlas::allocateTexture() {
    // Set up the storage
    glBindTexture(GL_TEXTURE_2D_ARRAY, mAtlasTexture);

    // TODO: Investigate mipmapping, see SOA
    // Set up all the mipmap storage
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, TEXTURE_ATLAS_WIDTH_PX, TEXTURE_ATLAS_WIDTH_PX, mPages.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // Set up tex parameters
    // No mipmapping
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, (int)0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LOD, (int)0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Anisotropic filtering
    float anisotropy;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
    glActiveTexture(GL_TEXTURE0);
    // Smooth texture params
//    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 //   glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
 //   glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

    // Unbind
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Check if we had any errors
    checkGlError("TextureAtlas::allocateTexture");
    mNeedsReallocate = false;
}

void TextureAtlas::writeDebugPages() {
    const unsigned width = TEXTURE_ATLAS_WIDTH_PX;
    const unsigned height = width;

    int bytesPerPage = width * height * sizeof(color4);
    ui8* pixels = new ui8[width * height * sizeof(color4) * mPages.size()];

    glBindTexture(GL_TEXTURE_2D_ARRAY, mAtlasTexture);
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (size_t i = 0; i < mPages.size(); i++) {
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels + i * bytesPerPage, width, height, 32 /*depthbytes*/, 4 * width, 0xFF, 0xFF00, 0xFF0000, 0x0);
        SDL_SaveBMP(surface, ("atlas" + std::to_string(i) + ".bmp").c_str());
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    delete[] pixels;
}