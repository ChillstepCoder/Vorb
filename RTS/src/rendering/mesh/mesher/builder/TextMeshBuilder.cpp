#include "stdafx.h"
#include "TextMeshBuilder.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"

TextMeshBuilder::TextMeshBuilder() {

}

TextMeshBuilder::~TextMeshBuilder() {

}


#define SUBMESH_INDEX_MAIN -1
constexpr ui32 MAX_SUBTEXTURES_PER_MESH = 255;


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

void TextMeshBuilder::addString(std::string_view str, const f32v3& rootPosition, const Font& font, f32 glyphHeight, const f32v2& offset2D, TextAlign align, const f32v4 clipRect /*= f32v4(-1000.0f, -1000.0f, 2000.0f, 2000.0f)*/, bool shouldWrap /*= true*/) {
    assert(align == TextAlign::CENTER); // TODO: IMPLEMENT

    f32v2 posOffset2D = offset2D;
    posOffset2D.y += getInitialYOffset(font, align, glyphHeight) * glyphHeight;
    if (posOffset2D.x < clipRect.x) posOffset2D.x = clipRect.x;

    const f32 glyphScale = glyphHeight / FONT_PX_SIZE;
    f32 gx = 0.0f;
    // TODO: No heap alloc?
    std::vector <std::vector<GlyphToRender> > rows(1); // Rows of glyphs
    std::vector <f32> rightEdges(1, 0.0f); // Right edge positions for rows
    int si;
    for (si = 0; si < str.size(); ++si) {
        char c = str[si];
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
                        while (str[si - numChars] != ' ' && si - numChars != 0) numChars++;

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
    mFontData.mGlyphs.reserve(mFontData.mGlyphs.size() + si);

    rightEdges.back() = gx;
    // Get y offset
    f32 yOff = getYOffset(rows.size(), align, glyphHeight);

    // Render each row
    for (size_t y = 0; y < rows.size(); y++) {
        for (auto& g : rows[y]) {
            f32v2 position = posOffset2D + f32v2(g.x + rightEdges[y] * X_OFF_MULTS[(int)align], yOff - y * glyphHeight);
            f32v2 dims = font.mGlyphs[g.gi].size * glyphScale;
            f32v4 uvRect = font.mGlyphs[g.gi].uvRect;
            // Clip the glyphs with clipRect
            computeClipping(clipRect, position, dims, uvRect);
            // Don't draw the glyph if its too small after clipping
            if (dims.x > 0.0f && dims.y > 0.0f) {
                // Add glyph
                mFontData.mGlyphs.emplace_back(GlyphData{ uvRect, rootPosition, 0, dims, position });
            }
        }
    }
}

void TextMeshBuilder::finishMesh(Mesh& mesh, MeshDrawMode drawMode) {
    // return blank mesh if we have no geometry
    if (mFontData.mGlyphs.empty()) {
        mesh.destroy();
        return;
    }
    // Always shared
    mesh.mGpuData.mFlags.setBit(MeshFlags::USING_SHARED_IBO);

    // Set bounds
    mesh.mBoundingSphere = mBoundingSphere;

    // Allocate all buffers if needed
    initMeshBuffers(mesh.mGpuData);

    // Upload data
    uploadBufferData(mesh.mGpuData, mFontData, drawMode);
    mFontData.clear();

}

void TextMeshBuilder::initMeshBuffers(MeshGpuData& subMesh) {
    // VAO
    if (subMesh.mVao == 0) {
        glCreateVertexArrays(1, &subMesh.mVao);
    }
    // SSBO
    if (subMesh.mSSBO != 0) {
        glDeleteBuffers(1, &subMesh.mSSBO);
    }
    glCreateBuffers(1, &subMesh.mSSBO);
    // IBO
    subMesh.mIndexType = MeshIndexType::INT;
    glVertexArrayElementBuffer(subMesh.mVao, ProceduralMeshBuilder::sQuadIboUI32);

    checkGlError("TextMeshBuilder::initMeshBuffers");
}

void TextMeshBuilder::uploadBufferData(MeshGpuData& subMesh, const FontMeshData& data, MeshDrawMode drawMode) {
    // IBO
    subMesh.mLODData.mTotalIndexCount = (ui32)data.mGlyphs.size() * 6u;

    // SSBO
    glNamedBufferStorage(subMesh.mSSBO, sizeof(GlyphData) * data.mGlyphs.size(), data.mGlyphs.data(), 0);
    checkGlError("TextMeshBuilder::uploadMeshData");
}
