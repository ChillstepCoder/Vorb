#include "stdafx.h"
#include "QuadMesh.h"

#include "Random.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "rendering/RenderStats.h"

// Must match glsl
constexpr ui32 MAX_UNIFORM_ARRAY_SIZE = 256; // TODO: Query hardware + defines? Need to assert if uniform buffer size < 16kb

const f32v2 CUBE_FACING_AXIS_DIRECTIONS[enum_cast(CubeFacing::COUNT)] = {
    f32v2(-1, 1), // LEFT
    f32v2(1,  1),  // FRONT
    f32v2(1,  1),  // RIGHT
    f32v2(-1, 1), // BACK
    f32v2(1,  1),  // TOP
    f32v2(-1, -1)   // BOTTOM
};
const f32v2 CUBE_FACING_AXIS_INITIAL_OFFSETS[enum_cast(CubeFacing::COUNT)] = {
    f32v2(1, 0), // LEFT
    f32v2(0, 0),  // FRONT
    f32v2(0, 0),  // RIGHT
    f32v2(1, 0), // BACK
    f32v2(0, 0),  // TOP
    f32v2(1, 1)   // BOTTOM
};

// Define all possible templates for Mesh class
// Each function definition should be proceeded by this
// 
// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;
static constexpr float EPSILON = 0.005f;

void QuadMesh::reserveQuadCount(size_t count) {
    mVertexData.reserve(count * 4u);
}

void QuadMesh::addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, CubeFacing axis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal) {

    mVertexData.resize(mVertexData.size() + 4);
    assert(!mVertexData.empty());
    TileVertex* verts = &mVertexData.back() - 3;

    const i32v2& xyAxis = CUBE_FACING_AXIS[enum_cast(axis)];
    const i8v3 normal(CUBE_FACING_NORMALS[enum_cast(axis)]);
    const i8v2 tangent(CUBE_FACING_TANGENTS[enum_cast(axis)]);
    const f32v2& xyAxisDirection = CUBE_FACING_AXIS_DIRECTIONS[enum_cast(axis)];
    const f32v2& initialOffsetMult = CUBE_FACING_AXIS_INITIAL_OFFSETS[enum_cast(axis)];

    // Center the sprite
    // TODO: This shouldnt be hard coded to xy
    //const f32v2 offset(-(float)((xyDims.x - 1) / 2) + xyOffset.x, xyOffset.y);
    tilePosition.x += xyOffset.x;
    tilePosition.y += xyOffset.y;

    // Offset for back faces so we can invert direction and have proper back face culling
    tilePosition[xyAxis.x] += xyDims.x * initialOffsetMult.x;
    tilePosition[xyAxis.y] += xyDims.y * initialOffsetMult.y;

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
        vbl.normal = normal;
        vbl.tangent = tangent;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos = tilePosition;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = color;
        vbr.atlasPage = spriteAtlasPage;
        vbr.pos[xyAxis.x] += (xyDims.x + EPSILON) * xyAxisDirection.x;
        vbr.normal = normal;
        vbr.tangent = tangent;
    }
    { // Top Right
        TileVertex& vtr = verts[2];
        vtr.pos = tilePosition;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = color;
        vtr.atlasPage = spriteAtlasPage;
        vtr.pos[xyAxis.x] += (xyDims.x + EPSILON) * xyAxisDirection.x;
        vtr.pos[xyAxis.y] += (xyDims.y + EPSILON) * xyAxisDirection.y;
        vtr.normal = normal;
        vtr.tangent = tangent;
    }
    { // Top Left
        TileVertex& vtl = verts[3];
        vtl.pos = tilePosition;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = color;
        vtl.atlasPage = spriteAtlasPage;
        vtl.pos[xyAxis.y] += (xyDims.y + EPSILON) * xyAxisDirection.y;
        vtl.normal = normal;
        vtl.tangent = tangent;
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

void QuadMesh::finishMesh(MeshDrawMode drawMode) {
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
            glVertexAttribPointer(*windAttribute, 1, GL_UNSIGNED_BYTE, true, sizeof(TileVertex), (void*)offsetof(TileVertex, windInfluence));
        }
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_BYTE, false, sizeof(TileVertex), (void*)offsetof(TileVertex, normal));
        }
        if (const VGAttribute* tangentAttribute = program.tryGetAttribute("vTangent")) {
            glVertexAttribPointer(*tangentAttribute, 2, GL_BYTE, false, sizeof(TileVertex), (void*)offsetof(TileVertex, tangent));
        }
    }
}

void BillboardMesh::reserveQuadCount(size_t count) {
    mVertexData.reserve(count * 4u);
}

void BillboardMesh::addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence, ui8 roughness) {
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
    const i16 halfX = (i16)(xyDims.x * 0.5f * BILLBOARD_VERTEX_XZOFFSET_COMPRESSION_RATIO);

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
        vbl.roughness = roughness;
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
        vbr.roughness = roughness;
    }

    const f32 topZ = tilePosition.z + xyDims.y;
    { // Top Right
        BillboardVertex& vtr = verts[2];
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
        vtr.roughness = roughness;
    }
    { // Top Left
        BillboardVertex& vtl = verts[3];
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
        vtl.roughness = roughness;
    }
    
}

void BillboardMesh::finishMesh(MeshDrawMode drawMode) {
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
        // TODO: no more string lookup attributes :C
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, rootPos));
        glVertexAttribPointer(program.getAttribute("vXZOffset"), 2, GL_SHORT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, xzOffset));
        if (const VGAttribute* attr = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*attr, 2, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, uvs));
        }
        if (const VGAttribute* attr = program.tryGetAttribute("vAtlasPage")) {
            glVertexAttribPointer(*attr, 1, GL_UNSIGNED_SHORT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, atlasPage));
        }
        if (const VGAttribute* attr = program.tryGetAttribute("vWindInfluence")) {
            glVertexAttribPointer(*attr, 1, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, windInfluence));
        }
        if (const VGAttribute* attr = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*attr, 4, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, color));
        }
        if (const VGAttribute* attr = program.tryGetAttribute("vRoughness")) {
            glVertexAttribPointer(*attr, 1, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, roughness));
        }
    }
}

void TBOBillboardMesh::beginMesh() {
    mTextureData.clear();
    mIsInProgress = true;
}

void TBOBillboardMesh::reserveQuadCount(size_t count) {
    mTextureData.reserve(count);
}

void TBOBillboardMesh::reserveAdditionalQuadCount(size_t count)
{
    mTextureData.reserve(mTextureData.size() + count);
}

void TBOBillboardMesh::addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence, ui8 roughness) {
    UNUSED(color, xyOffset);
    // Signal for a new batch
    if (mTextureData.empty()) {
        mTypes.clear();
    }

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

    TBOBillboardUniformData uniformData = TBOBillboardUniformData{ adjustedUvs, f32v3((f32)spriteAtlasPage, windInfluence / 255.0f, roughness / 255.0f) };

    f32 type;
    auto&& it = mTypes.find(uniformData);
    if (it == mTypes.end()) {
        size_t index = mTypes.size();
        if (index == MAX_UNIFORM_ARRAY_SIZE /*max types per batch*/) {
            assert(false); // Too many!
            return;
        }
        type = (f32)index;
        mTypes[uniformData] = (ui32)index;
    }
    else {
        type = (f32)it->second;
    }

    mTextureData.push_back({ tilePosition, type, xyDims.x, xyDims.y });
}

void TBOBillboardMesh::draw(const vg::GLProgram& program) const {


    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);
    bindVertexAttribs(program);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Hack, no data at all, the shader generates vertex positions
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3); 

    glBindVertexArray(0);
}

void TBOBillboardMesh::finishMesh(MeshDrawMode drawMode)
{
    if (mTextureData.size()) {
        mIndexCount = mTextureData.size() * 6;
        int maxSize;
        //glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxSize);
        const size_t size = sizeof(TBOBillboardInstanceData) * mTextureData.size();
       // assert(size < maxSize);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_TEXTURE_BUFFER, mVbo);
        glBufferData(GL_TEXTURE_BUFFER, size, nullptr, (GLenum)drawMode);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, size, mTextureData.data());

        if (!mTboTexture) {
            glGenTextures(1, &mTboTexture);
            glBindTexture(GL_TEXTURE_BUFFER, mTboTexture);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, mVbo);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        if (mDepthSortMode == DepthSortMode::NONE) {
            std::vector<TBOBillboardInstanceData>().swap(mTextureData);
        }

        // Set uniforms
        if (!mUbo) {
            glGenBuffers(1, &mUbo);
            glBindBuffer(GL_UNIFORM_BUFFER, mUbo);
            // 2 arrays of data
            glBufferData(GL_UNIFORM_BUFFER, MAX_UNIFORM_ARRAY_SIZE * sizeof(f32v4) * 2, nullptr, GL_STATIC_DRAW); // allocate 152 bytes of memory
        }
        else {
            glBindBuffer(GL_UNIFORM_BUFFER, mUbo);
        }
        // TODO: Dont do this every sort update
        for (auto&& it = mTypes.begin(); it != mTypes.end(); ++it) {
            ui32 index = it->second;
            // base alignment is 16 for uniform block
            glBufferSubData(GL_UNIFORM_BUFFER, index * sizeof(f32v4), sizeof(f32v4), &it->first.uvRect.x);
            glBufferSubData(GL_UNIFORM_BUFFER, MAX_UNIFORM_ARRAY_SIZE * sizeof(f32v4) + index * sizeof(f32v4), sizeof(f32v3), &it->first.atlasPageRoughnessWind.x);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    else {
        destroy();
    }
    mIsInProgress = false;
}

void TBOBillboardMesh::destroy() {
    MeshBase::destroy();
    if (mTboTexture) {
        glDeleteTextures(1, &mTboTexture);
        mTboTexture = 0;
    }
    if (mUbo) {
        glDeleteBuffers(1, &mUbo);
        mUbo = 0;
    }
    mTypes.clear();
    std::vector<TBOBillboardInstanceData>().swap(mTextureData);
}

void TBOBillboardMesh::clearForRecycleRetainMemory() {
    mTypes.clear();
    mTextureData.clear();
    mIndexCount = 0;
}

void TBOBillboardMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_BUFFER, mTboTexture);

    // Bind uniforms
    // GLSL ensures binding point 1
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, mUbo);
    glUniform1i(program.getUniform("UnTboPositionTypeSize"), 10);
}

void GrassBillboardMesh::reserveQuadCount(size_t count)
{
    mInstanceData.reserve(count);
    mPositionData.reserve(count);
}

void GrassBillboardMesh::addBladeQuad(const f32v3& position, const f32v2& xyDims, const ui8v3& color)
{
    assert(xyDims.x <= 1.0f && xyDims.y <= 1.0f);

    mInstanceData.emplace_back(color, ui8v2(xyDims.x * 255.0f, xyDims.y * 255.0f));
    mPositionData.emplace_back(position);
}

void GrassBillboardMesh::draw(const vg::GLProgram& program) const
{
    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);
    bindVertexAttribs(program);

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Hack, no data at all, the shader generates vertex positions
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}

void GrassBillboardMesh::finishMesh(MeshDrawMode drawMode)
{
    if (mInstanceData.size()) {

        if (mVboPosition == 0) {
            glGenBuffers(1, &mVboPosition);
            glBindBuffer(GL_TEXTURE_BUFFER, mVboPosition);
            glBindVertexArray(0);
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        mIndexCount = mInstanceData.size() * 6;
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_TEXTURE_BUFFER, mVboPosition);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(f32v3) * mPositionData.size(), nullptr, (GLenum)drawMode);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(f32v3) * mPositionData.size(), mPositionData.data());

        glBindBuffer(GL_TEXTURE_BUFFER, mVbo);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(GrassBillboardInstanceData) * mInstanceData.size(), nullptr, (GLenum)drawMode);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(GrassBillboardInstanceData) * mInstanceData.size(), mInstanceData.data());


        if (!mTboInstanceData) {
            glGenTextures(1, &mTboInstanceData);
            glBindTexture(GL_TEXTURE_BUFFER, mTboInstanceData);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, mVbo);
            glGenTextures(1, &mTboPositionData);
            glBindTexture(GL_TEXTURE_BUFFER, mTboPositionData);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, mVboPosition);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }
    else {
        destroy();
    }
    std::vector<GrassBillboardInstanceData>().swap(mInstanceData);
    std::vector<f32v3>().swap(mPositionData);
}

void GrassBillboardMesh::destroy()
{
    if (mVboPosition != 0) {
        glDeleteBuffers(1, &mVboPosition);
        mVboPosition = 0;
    }
    MeshBase::destroy();
}

void GrassBillboardMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_BUFFER, mTboInstanceData);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_BUFFER, mTboPositionData);

    // Bind uniforms
    glUniform1i(program.getUniform("UnTboColorSize"), 10);
    glUniform1i(program.getUniform("UnTboPosition"), 11);
}
