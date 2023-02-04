#include "stdafx.h"
#include "FontRepository.h"

#include <Vorb/io/FileOps.h>

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/GraphicsDevice.h>
#include <SDL_ttf/SDL_ttf.h>

#define FIRST_PRINTABLE_CHAR ((char)32)
#define LAST_PRINTABLE_CHAR ((char)126)

i32 closestPowerOf2(i32 i) {
    i--;
    i32 pi = 1;
    while (i > 0) {
        i >>= 1;
        pi <<= 1;
    }
    return pi;
}

bool FontRepository::loadFont(const vio::Path& fontPath)
{
    TTF_Font* f = TTF_OpenFont(fontPath.getCString(), FONT_PX_SIZE);
    const nString fontName = vio::getLeafNameFromFilePathNoExtension(fontPath);
    if (!f) {
        std::cerr << "Failed to open font " << fontPath.getCString() << "\n";
        std::cerr << "Error: " << TTF_GetError() << std::endl;
        return false;
    }
    Font& font = mFonts[fontName];
    font.mFontHeight = TTF_FontHeight(f);
    SDL_Color fg = { 255, 255, 255, 255 };

    ui32 padding = FONT_PX_SIZE / 8;

    // First Measure All The Regions
    const size_t glyphCount = LAST_PRINTABLE_CHAR - FIRST_PRINTABLE_CHAR + 1;
    font.mGlyphs.resize(glyphCount + 1); // +1 for null character at end or something
    std::vector<i32v4> glyphRects(glyphCount);

    i32 advance;
    for (size_t i = 0; i < font.mGlyphs.size() - 1; ++i) {
        auto&& glyph = font.mGlyphs[i];
        char c = i + FIRST_PRINTABLE_CHAR;
        glyph.character = c;
        TTF_GlyphMetrics(f, c, &glyphRects[i].x, &glyphRects[i].z, &glyphRects[i].y, &glyphRects[i].w, &advance);
        glyphRects[i].z -= glyphRects[i].x;
        glyphRects[i].x = 0;
        glyphRects[i].w -= glyphRects[i].y;
        glyphRects[i].y = 0;
    }

    // Find Best Partitioning Of Glyphs
    ui32 rows = 1, w, h, bestWidth = 0, bestHeight = 0, area = 4096 * 4096, bestRows = 0;
    std::vector<ui32>* bestPartition = nullptr;
    while (rows <= glyphCount) {
        h = rows * (padding + font.mFontHeight) + padding;
        auto gr = createRows(glyphRects, rows, padding, w);

        // Desire A Power Of 2 Texture
        w = closestPowerOf2(w);
        h = closestPowerOf2(h);

        // A Texture Must Be Feasible
        ui32 maxTextureSize = vg::GraphicsDevice::getCurrent()->getProperties().maxTextureSize;
        if (w > maxTextureSize || h > maxTextureSize) {
            rows++;
            delete[] gr;
            assert(false && "Graphics device doesnt support enough texture size for font");
            continue;
        }

        // Check For Minimal Area
        if (area >= w * h) {
            if (bestPartition) delete[] bestPartition;
            bestPartition = gr;
            bestWidth = w;
            bestHeight = h;
            bestRows = rows;
            area = bestWidth * bestHeight;
            rows++;
        }
        else {
            delete[] gr;
            break;
        }
    }

    // Can A Bitmap Font Be Made?
    if (!bestPartition) {
        pError("Could not find valid partition for font " + fontName);
        return false;
    }

    // Miplevels
    i32 maxMipmapLevels = 0;
    i32 size = glm::min(bestWidth, bestHeight);
    while (size > 1) {
        maxMipmapLevels++;
        size >>= 1;
    }

    // Create The Texture
    glCreateTextures(GL_TEXTURE_2D, 1, &font.mTexture);
    glTextureStorage2D(font.mTexture, maxMipmapLevels, GL_RGBA8, bestWidth, bestHeight);

    // Now Draw All The Glyphs
    ui32 ly = padding;
    for (size_t ri = 0; ri < bestRows; ri++) {
        ui32 lx = padding;
        for (size_t ci = 0; ci < bestPartition[ri].size(); ci++) {
            ui32 gi = bestPartition[ri][ci];

            SDL_Surface* glyphSurface = TTF_RenderGlyph_Blended(f, (char)(font.mGlyphs[0].character + gi), fg);
            // Pre-multiplication Occurs Here
            ubyte* sp = (ubyte*)glyphSurface->pixels;
            ui32 cp = glyphSurface->w * glyphSurface->h * 4;
            for (size_t i = 0; i < cp; i += 4) {
                f32 a = sp[i + 3] / 255.0f;
                sp[i] = (ubyte)((f32)sp[i] * a);
                sp[i + 1] = sp[i];
                sp[i + 2] = sp[i];
            }

            // Save Glyph Image And Update Coordinates
            glTextureSubImage2D(font.mTexture, 0, lx, ly, glyphSurface->w, glyphSurface->h, GL_BGRA, GL_UNSIGNED_BYTE, glyphSurface->pixels);
            glyphRects[gi].x = lx;
            glyphRects[gi].y = ly;
            glyphRects[gi].z = glyphSurface->w;
            glyphRects[gi].w = glyphSurface->h;

            SDL_FreeSurface(glyphSurface);
            glyphSurface = nullptr;

            lx += glyphRects[gi].z + padding;
        }
        ly += font.mFontHeight + padding;
    }

    // Draw The Unsupported Glyph
    ui32 rs = padding - 1;
    ui32* pureWhiteSquare = new ui32[rs * rs];
    memset(pureWhiteSquare, 0xffffffffu, rs * rs * sizeof(ui32));
    glTextureSubImage2D(font.mTexture, 0, 0, 0, rs, rs, GL_RGBA, GL_UNSIGNED_BYTE, pureWhiteSquare);
    delete[] pureWhiteSquare;
    pureWhiteSquare = nullptr;

    // Set size and uvs
    for (size_t i = 0; i < font.mGlyphs.size() - 1; i++) {
        font.mGlyphs[i].size = f32v2((f32)glyphRects[i].z, (f32)glyphRects[i].w);
        font.mGlyphs[i].uvRect = f32v4(
            (f32)glyphRects[i].x / (f32)bestWidth,
            (f32)glyphRects[i].y / (f32)bestHeight,
            (f32)glyphRects[i].z / (f32)bestWidth,
            (f32)glyphRects[i].w / (f32)bestHeight
        );
    }
    font.mGlyphs.back().character = ' ';
    font.mGlyphs.back().size = font.mGlyphs[0].size;
    font.mGlyphs.back().uvRect = f32v4(0.0f, 0.0f, (f32)rs / (f32)bestWidth, (f32)rs / (f32)bestHeight);

    //#ifdef DEBUG
    //    // Save An Image
    //    std::vector<ui8> pixels;
    //    pixels.resize(bestWidth * bestHeight * 4);
    //    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
    //    char buffer[512];
    //    sprintf(buffer, "SFont_%s_%s_%d.png", TTF_FontFaceFamilyName(f), TTF_FontFaceStyleName(f), size);
    //    vg::ImageIO().save(buffer, pixels.data(), bestWidth, bestHeight, vg::ImageIOFormat::RGBA_UI8);
    //#endif // DEBUG


    // Sampler state and mipmap
    vg::sSamplerStates.LINEAR_CLAMP_MIPMAP.setForTexture(font.mTexture);
    glTextureParameteri(font.mTexture, GL_TEXTURE_MAX_LOD, maxMipmapLevels);
    glTextureParameteri(font.mTexture, GL_TEXTURE_MAX_LEVEL, maxMipmapLevels);
    glGenerateTextureMipmap(font.mTexture);

    // Make resident handle


    TTF_CloseFont(f);

    delete[] bestPartition;

    checkGlError("FontRepository::loadFont");
}

const Font& FontRepository::getFont(const cString fontName)
{
    auto&& it = mFonts.find(fontName);
    assert(it != mFonts.end() && "Missing font");
    return it->second;
}

std::vector<ui32>* FontRepository::createRows(const std::vector<i32v4>& rects, ui32 r, ui32 padding, ui32& w) {
    // Blank Initialize
    std::vector<ui32>* l = new std::vector<ui32>[r]();
    ui32* cw = new ui32[r]();
    for (size_t i = 0; i < r; i++) {
        cw[i] = padding;
    }

    // Loop Through All Glyphs
    for (ui32 i = 0; i < rects.size(); i++) {
        // Find Row For Placement
        size_t ri = 0;
        for (size_t rii = 1; rii < r; rii++)
            if (cw[rii] < cw[ri]) ri = rii;

        // Add Width To That Row
        cw[ri] += rects[i].z + padding;

        // Add Glyph To The Row List
        l[ri].push_back(i);
    }

    // Find The Max Width
    w = 0;
    for (size_t i = 0; i < r; i++) {
        if (cw[i] > w) w = cw[i];
    }

    delete[] cw;
    return l;
}
