#include "stdafx.h"
#include "QuadMesh.h"

#include "world/Chunk.h"
#include "rendering/RenderContext.h"
#include "Random.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

// Define all possible templates for Mesh class
// Each function definition should be proceeded by this
// 
// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;
static constexpr float EPSILON = 0.005f;

VGBuffer MeshBase::sIbo = 0;

MeshBase::MeshBase() {
    init();
}

MeshBase::~MeshBase() {
    destroy();
}

void MeshBase::initStaticIBO() {
    if (sIbo) {
        return;
    }

    ui32 i = 0;
    std::vector<ui32> quadIndices(MAX_MESH_INDICES);
    for (ui32 v = 0; i < MAX_MESH_INDICES; v += 4u) {
        quadIndices[i++] = v;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v + 1;
        quadIndices[i++] = v;
    }

    glGenBuffers(1, &sIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_MESH_INDICES * sizeof(ui32), quadIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MeshBase::init() {
    if (mVao == 0) { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        assert(sIbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIbo);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void MeshBase::destroy() {
    if (mVbo != 0) {
        glDeleteBuffers(1, &mVbo);
        mVbo = 0;
    }
    if (mVao != 0) {
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }

    mIndexCount = 0;
}

void MeshBase::draw(const vg::GLProgram& program) const {
    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);
    bindVertexAttribs(program);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);

    glBindVertexArray(0);
}

void QuadMesh::reserveQuadCount(size_t count) {
    mVertexData.reserve(count * 4u);
}

void QuadMesh::addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, const i32v2& xyAxis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal) {

    mVertexData.resize(mVertexData.size() + 4);
    assert(!mVertexData.empty());
    TileVertex* verts = &mVertexData.back() - 3;

    // Center the sprite
    // TODO: This shouldnt be hard coded to xy
    const f32v2 offset(-(float)((xyDims.x - 1) / 2) + xyOffset.x, xyOffset.y);
    tilePosition.x += offset.x;
    tilePosition.y += offset.y;

    f32v4 adjustedUvs;
    if (shouldRandFlipHorizontal && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    { // Bottom Left
        TileVertex& vbl = verts[0];
        vbl.pos = tilePosition;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = color;
        vbl.atlasPage = spriteAtlasPage;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos = tilePosition;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = color;
        vbr.atlasPage = spriteAtlasPage;
        vbr.pos[xyAxis.x] += xyDims.x + EPSILON;
    }

    { // Top Left
        TileVertex& vtl = verts[2];
        vtl.pos = tilePosition;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = color;
        vtl.atlasPage = spriteAtlasPage;
        vtl.pos[xyAxis.y] += xyDims.y + EPSILON;
    }
    { // Top Right
        TileVertex& vtr = verts[3];
        vtr.pos = tilePosition;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = color;
        vtr.atlasPage = spriteAtlasPage;
        vtr.pos[xyAxis.x] += xyDims.x + EPSILON;
        vtr.pos[xyAxis.y] += xyDims.y + EPSILON;
    }
}

void QuadMesh::addCross(f32v3 cornerPosition, ui16 spriteAtlasPage, const f32v4& uvs, float width, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence) {
    mVertexData.resize(mVertexData.size() + 8);
    TileVertex* verts = &mVertexData.back() - 7;

    f32v4 adjustedUvs;
    if (shouldRandFlipHorizontal && Random::getThreadSafef(cornerPosition.x, cornerPosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor = topColor;

    for (int i = 0; i < 2; ++i) {
        f32 offset = i * width;
        f32 invOffset = width - offset;
        { // Bottom Left
            TileVertex& vbl = *(verts++);
            vbl.pos = cornerPosition;
            vbl.uvs.x = adjustedUvs.x;
            vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbl.color = color;
            vbl.atlasPage = spriteAtlasPage;
            vbl.pos.y += offset;
            vbl.windInfluence = 0;
        }
        { // Bottom Right
            TileVertex& vbr = *(verts++);
            vbr.pos = cornerPosition;
            vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbr.color = color;
            vbr.atlasPage = spriteAtlasPage;
            vbr.pos.x += width;
            vbr.pos.y += invOffset;
            vbr.windInfluence = 0;
        }

        { // Top Left
            TileVertex& vtl = *(verts++);
            vtl.pos = cornerPosition;
            vtl.uvs.x = adjustedUvs.x;
            vtl.uvs.y = adjustedUvs.y;
            vtl.color = color;
            vtl.atlasPage = spriteAtlasPage;
            vtl.pos.y += offset;
            vtl.pos.z += width;
            vtl.windInfluence = windInfluence;
        }
        { // Top Right
            TileVertex& vtr = *(verts++);
            vtr.pos = cornerPosition;
            vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vtr.uvs.y = adjustedUvs.y;
            vtr.color = color;
            vtr.atlasPage = spriteAtlasPage;
            vtr.pos.x += width;
            vtr.pos.y += invOffset;
            vtr.pos.z += width;
            vtr.windInfluence = windInfluence;
        }
    }
}

void QuadMesh::finishMesh(QuadMeshDrawMode drawMode) {
    if (mVertexData.size()) {
        setData(mVertexData.data(), mVertexData.size(), drawMode);
        std::vector<TileVertex>().swap(mVertexData);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
}

void QuadMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 2, GL_FLOAT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, uvs));
        }
        if (const VGAttribute* atlasAttribute = program.tryGetAttribute("vAtlasPage")) {
            glVertexAttribPointer(*atlasAttribute, 1, GL_UNSIGNED_SHORT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, atlasPage));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(TileVertex), (void*)offsetof(TileVertex, color));
        }
        if (const VGAttribute* windAttribute = program.tryGetAttribute("vWindInfluence")) {
            glVertexAttribPointer(program.getAttribute("vWindInfluence"), 1, GL_UNSIGNED_BYTE, true, sizeof(TileVertex), (void*)offsetof(TileVertex, windInfluence));
        }
    }
}

void BillboardMesh::reserveQuadCount(size_t count) {
    mVertexData.reserve(count * 4u);
}

void BillboardMesh::addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence) {
    mVertexData.resize(mVertexData.size() + 4);
    BillboardVertex* verts = &mVertexData.back() - 3;

    f32v4 adjustedUvs;
    if (shouldRandFlipHorizontal && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    i16v2 compressedOffset = i16v2(xyOffset * BILLBOARD_VERTEX_XZOFFSET_COMPRESSION_RATIO);
    i16v2 compressedDims = i16v2(xyDims * BILLBOARD_VERTEX_XZOFFSET_COMPRESSION_RATIO);
    const i16 halfX = xyDims.x * 0.5f * BILLBOARD_VERTEX_XZOFFSET_COMPRESSION_RATIO;

    { // Bottom Left
        BillboardVertex& vbl = verts[0];
        vbl.rootPos.x = tilePosition.x;
        vbl.rootPos.y = tilePosition.y;
        vbl.rootPos.z = tilePosition.z;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = color;
        vbl.atlasPage = spriteAtlasPage;
        vbl.xzOffset.x = compressedOffset.x - halfX;
        vbl.xzOffset.y = compressedOffset.y;
    }
    { // Bottom Right
        BillboardVertex& vbr = verts[1];
        vbr.rootPos.x = tilePosition.x;
        vbr.rootPos.y = tilePosition.y;
        vbr.rootPos.z = tilePosition.z;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = color;
        vbr.atlasPage = spriteAtlasPage;
        vbr.xzOffset.x = compressedOffset.x + halfX;
        vbr.xzOffset.y = compressedOffset.y;
    }

    const f32 topZ = tilePosition.z + xyDims.y;
    { // Top Left
        BillboardVertex& vtl = verts[2];
        vtl.rootPos.x = tilePosition.x;
        vtl.rootPos.y = tilePosition.y;
        vtl.rootPos.z = tilePosition.z;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = color;
        vtl.atlasPage = spriteAtlasPage;
        vtl.xzOffset.x = compressedOffset.x - halfX;
        vtl.xzOffset.y = compressedOffset.y + compressedDims.y;
        vtl.windInfluence = windInfluence;
    }
    { // Top Right
        BillboardVertex& vtr = verts[3];
        vtr.rootPos.x = tilePosition.x;
        vtr.rootPos.y = tilePosition.y;
        vtr.rootPos.z = tilePosition.z;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = color;
        vtr.atlasPage = spriteAtlasPage;
        vtr.xzOffset.x = compressedOffset.x + halfX;
        vtr.xzOffset.y = compressedOffset.y + compressedDims.y;
        vtr.windInfluence = windInfluence;
    }
}

void BillboardMesh::finishMesh(QuadMeshDrawMode drawMode) {
    if (mVertexData.size()) {
        setData(mVertexData.data(), mVertexData.size(), drawMode);
        std::vector<BillboardVertex>().swap(mVertexData);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
}

void BillboardMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, rootPos));
        glVertexAttribPointer(program.getAttribute("vXZOffset"), 2, GL_SHORT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, xzOffset));
        glVertexAttribPointer(program.getAttribute("vUV"), 2, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, uvs));
        glVertexAttribPointer(program.getAttribute("vAtlasPage"), 1, GL_UNSIGNED_SHORT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, atlasPage));
        glVertexAttribPointer(program.getAttribute("vWindInfluence"), 1, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, windInfluence));

        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, color));
        }
    }
}
