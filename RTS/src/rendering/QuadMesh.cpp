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

const f32v2 CUBE_FACING_AXIS_DIRECTIONS[e_cast(CubeFacing::COUNT)] = {
    f32v2(-1, 1), // LEFT
    f32v2(1,  1),  // FRONT
    f32v2(1,  1),  // RIGHT
    f32v2(-1, 1), // BACK
    f32v2(1,  1),  // TOP
    f32v2(-1, -1)   // BOTTOM
};
const f32v2 CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(CubeFacing::COUNT)] = {
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

    const i32v2& xyAxis = CUBE_FACING_AXIS[e_cast(axis)];
    const i8v3 normal(CUBE_FACING_NORMALS[e_cast(axis)]);
    const i8v2 tangent(CUBE_FACING_TANGENTS[e_cast(axis)]);
    const f32v2& xyAxisDirection = CUBE_FACING_AXIS_DIRECTIONS[e_cast(axis)];
    const f32v2& initialOffsetMult = CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(axis)];

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

void QuadMesh::addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool flipTriangleDir, bool shouldRandFlipHorizontal) {
    mVertexData.resize(mVertexData.size() + 4);
    assert(!mVertexData.empty());
    TileVertex* verts = &mVertexData.back() - 3;

    // TODO: This is a bad approximation
    const f32 dX = ((terrainCorners[0] - terrainCorners[1]) + (terrainCorners[2] - terrainCorners[3])) * 0.5f;
    const f32 dY = ((terrainCorners[0] - terrainCorners[2]) + (terrainCorners[1] - terrainCorners[3])) * 0.5f;
    const f32 dZ = 1.0f;

    f32v3 n(dX, dY, dZ);
    i8v3 normal(glm::normalize(n) * 127.0f);

    const i8v2 tangent(0, 1);

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

    if (flipTriangleDir) {
        { // Bottom Right
            TileVertex& vbr = verts[0];
            vbr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON);
            vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbr.color = color;
            vbr.atlasPage = spriteAtlasPage;
            vbr.normal = normal;
            vbr.tangent = tangent;
        }
        { // Top Right
            TileVertex& vtr = verts[1];
            vtr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON);
            vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vtr.uvs.y = adjustedUvs.y;
            vtr.color = color;
            vtr.atlasPage = spriteAtlasPage;
            vtr.normal = normal;
            vtr.tangent = tangent;
        }
        { // Top Left
            TileVertex& vtl = verts[2];
            vtl.pos = f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON);
            vtl.uvs.x = adjustedUvs.x;
            vtl.uvs.y = adjustedUvs.y;
            vtl.color = color;
            vtl.atlasPage = spriteAtlasPage;
            vtl.normal = normal;
            vtl.tangent = tangent;
        }
        { // Bottom Left
            TileVertex& vbl = verts[3];
            vbl.pos = f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON);
            vbl.uvs.x = adjustedUvs.x;
            vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbl.color = color;
            vbl.atlasPage = spriteAtlasPage;
            vbl.normal = normal;
            vbl.tangent = tangent;
        }
    } else {

        { // Bottom Left
            TileVertex& vbl = verts[0];
            vbl.pos = f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON);
            vbl.uvs.x = adjustedUvs.x;
            vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbl.color = color;
            vbl.atlasPage = spriteAtlasPage;
            vbl.normal = normal;
            vbl.tangent = tangent;
        }
        { // Bottom Right
            TileVertex& vbr = verts[1];
            vbr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON);
            vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbr.color = color;
            vbr.atlasPage = spriteAtlasPage;
            vbr.normal = normal;
            vbr.tangent = tangent;
        }
        { // Top Right
            TileVertex& vtr = verts[2];
            vtr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON);
            vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vtr.uvs.y = adjustedUvs.y;
            vtr.color = color;
            vtr.atlasPage = spriteAtlasPage;
            vtr.normal = normal;
            vtr.tangent = tangent;
        }
        { // Top Left
            TileVertex& vtl = verts[3];
            vtl.pos = f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON);
            vtl.uvs.x = adjustedUvs.x;
            vtl.uvs.y = adjustedUvs.y;
            vtl.color = color;
            vtl.atlasPage = spriteAtlasPage;
            vtl.normal = normal;
            vtl.tangent = tangent;
        }
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
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
    std::vector<TileVertex>().swap(mVertexData);
}

void QuadMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != program.getID()) {
        mLastUsedProgram = program.getID();

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

void GrassBillboardMesh::reserveQuadCount(size_t count)
{
    mInstanceData.reserve(count);
    mPositionData.reserve(count);
}

void GrassBillboardMesh::addBladeQuad(const f32v3& position, const f32v2& xyDims, ui8 grassType)
{
    assert(xyDims.x <= 1.0f && xyDims.y <= 1.0f);

    mInstanceData.emplace_back(ui8v2(xyDims.x * 255.0f, xyDims.y * 255.0f), grassType);
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
    // TODO: Only once
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawElements(GL_PATCHES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}

void GrassBillboardMesh::finishMesh(MeshDrawMode drawMode)
{
    if (mInstanceData.size()) {
        lazyInitBuffers();

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
    glUniform1i(program.getUniform("UnTboSizeType"), 10);
    glUniform1i(program.getUniform("UnTboPosition"), 11);
}
