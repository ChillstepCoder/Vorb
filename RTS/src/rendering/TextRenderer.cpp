#include "stdafx.h"
#include "TextRenderer.h"

#include "rendering/mesh/BillboardMeshBuilder.h"
#include "rendering/MaterialManager.h"

#include "ResourceManager.h"
#include "resources/FontRepository.h"


TextMeshBillboard::TextMeshBillboard() {

}

TextMeshBillboard::~TextMeshBillboard() {

}

// X Offset multipliers for TextAlign
const f32 X_OFF_MULTS[10] = {
    0.0f, // NONE
    0.0f, // LEFT
    0.0f, // TOP_LEFT
    -0.5f, // TOP
    -1.0f, // TOP_RIGHT
    -1.0f, // RIGHT
    -1.0f, // BOTTOM_RIGHT
    -0.5f, // BOTTOM
    0.0f, // BOTTOM_LEFT
    -0.5f // CENTER
};

// Used for alignment
struct GlyphToRender {
    GlyphToRender(size_t gi, f32 x) : gi(gi), x(x) {}
    size_t gi;
    f32 x;
};


TextRenderer::TextRenderer(const MaterialRenderer& materialRenderer) : mMaterialRenderer(materialRenderer) {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mBillboardMaterial = materialManager.getMaterial("billboard_ssbo");
}


f32 getInitialYOffset(const Font& font, TextAlign textAlign, f32 glyphHeight) {
    // No need to measure top left
    if (textAlign == TextAlign::TOP_LEFT) return 0.0f;
    switch (textAlign) {
        case TextAlign::LEFT:
            return -glyphHeight / 2.0f;
        case TextAlign::TOP_LEFT:
            return 0.0f;
        case TextAlign::TOP:
            return 0.0f;
        case TextAlign::TOP_RIGHT:
            return 0.0f;
        case TextAlign::RIGHT:
            return -glyphHeight / 2.0f;
        case TextAlign::BOTTOM_RIGHT:
            return -glyphHeight;
        case TextAlign::BOTTOM:
            return -glyphHeight;
        case TextAlign::BOTTOM_LEFT:
            return -glyphHeight;
        case TextAlign::CENTER:
            return -glyphHeight / 2.0f;
        default:
            return 0.0f; // Should never happen
    }
    return 0.0f;
}

f32 getYOffset(size_t numRows, TextAlign align, f32 glyphHeight) {
    switch (align) {
        case TextAlign::TOP_LEFT:
        case TextAlign::TOP:
        case TextAlign::TOP_RIGHT:
            return 0.0f;
        case TextAlign::LEFT:
        case TextAlign::CENTER:
        case TextAlign::RIGHT:
            return -((f32)(numRows - 1) * glyphHeight / 2.0f);
        default:
            return -((f32)(numRows - 1) * glyphHeight);
    }
    return 0.0f; // Should never happen
}



void TextRenderer::createTextMesh(Font& font, TextMeshBillboard& mesh, const cString text, const f32v3& rootPosWorld, const f32v2& offset2D, const f32 glyphHeight, const color4& color, TextAlign align /*= TextAlign::CENTER*/, const f32v4& clipRect, bool shouldWrap /*= false*/) {
    assert(align == TextAlign::CENTER); // TODO: IMPLEMENT

    BillboardMeshBuilder meshBuilder;

    mesh.mRootPosition = rootPosWorld;

    f32v2 posOffset2D = offset2D;
    posOffset2D.y += getInitialYOffset(font, align, glyphHeight) * glyphHeight;
    if (posOffset2D.x < clipRect.x) posOffset2D.x = clipRect.x;

    const f32 glyphScale = glyphHeight / FONT_PX_SIZE;
    f32 gx = 0.0f;
    // TODO: No heap alloc?
    std::vector <std::vector<GlyphToRender> > rows(1); // Rows of glyphs
    std::vector <f32> rightEdges(1, 0.0f); // Right edge positions for rows
    int si;
    for (si = 0; text[si] != 0; ++si) {
        char c = text[si];
        if (c == '\n') {
            // Go to new row on newlines
            rightEdges.back() = gx;
            rightEdges.push_back(0.0f);
            rows.emplace_back();
            gx = 0.0f;
        }
        else {
            bool useGlyph = true;
            // Check For Correct Glyph
            size_t gi = c - font.mGlyphs[0].character;
            if (gi >= font.mGlyphs.size()) gi = font.mGlyphs.size() - 1;

            // Get glyph width
            f32 gWidth = font.mGlyphs[gi].size.x * glyphScale;

            // Check for wrapping
            if (shouldWrap) {
                bool isOut;
                switch (align) {
                    case TextAlign::TOP:
                    case TextAlign::CENTER:
                    case TextAlign::BOTTOM:
                        isOut = ((posOffset2D.x + (gx + gWidth) / 2.0f > clipRect.x + clipRect.z)); break;
                    case TextAlign::TOP_RIGHT:
                    case TextAlign::RIGHT:
                    case TextAlign::BOTTOM_RIGHT:
                        isOut = ((posOffset2D.x - gx - gWidth < clipRect.x)); break;
                    default:
                        isOut = ((posOffset2D.x + gx + gWidth > clipRect.x + clipRect.z)); break;
                }

                // If the glyph is out of the clip rect, may need to go to new row
                if (isOut) {
                    // TODO(Ben): Check input clipping characters
                    if (c == ' ') {
                        useGlyph = false;
                    }
                    else {
                        // Count the word size
                        int numChars = 0;
                        while (text[si - numChars] != ' ' && si - numChars != 0) numChars++;

                        if (si - numChars > 0) {
                            for (int i = 0; i < numChars; i++) {
                                gx -= font.mGlyphs[si - i].size.x * glyphScale;
                            }
                            si -= (numChars + 1); // -1 to counter ++ later.
                            rightEdges.back() = gx;
                            gx = 0.0f;
                            useGlyph = false; // TODO(Ben): Is this right?
                        }
                        else {
                            rightEdges.back() = gx;
                        }
                    }
                    // Go to new row
                    rightEdges.push_back(0.0f);
                    rows.emplace_back();
                    gx = 0.0f;
                }
            }
            // Add glyph to the row
            if (useGlyph) {
                rows.back().emplace_back(gi, gx);
                gx += gWidth;
            }
        }
    }

    // Reserve for efficiency
    meshBuilder.reserveBillboardCount(si);

    rightEdges.back() = gx;
    // Get y offset
    f32 yOff = getYOffset(rows.size(), align, glyphHeight);
    // Render each row
    for (size_t y = 0; y < rows.size(); y++) {
        // TODO(Matthew): This value isn't used, use or kill.
        //f32 rightEdge = rightEdges[y];
        for (auto& g : rows[y]) {
            f32v2 position = posOffset2D + f32v2(g.x + rightEdges[y] * X_OFF_MULTS[(int)align], yOff + y * glyphHeight);
            f32v2 dims = font.mGlyphs[g.gi].size * glyphScale;
            f32v4 uvRect = font.mGlyphs[g.gi].uvRect;
            // Clip the glyphs with clipRect
            computeClipping(clipRect, position, dims, uvRect);
            // Don't draw the glyph if its too small after clipping
            if (dims.x > 0.0f && dims.y > 0.0f) {
                // Add glyph
                meshBuilder.addBillboard(f32v3(position.x, position.y, 0.0f), dims, font.mTexture);
            }
        }
    }

    meshBuilder.finishMesh(mesh.mMesh, MeshDrawMode::STATIC);
}
