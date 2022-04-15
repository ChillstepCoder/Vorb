#include "stdafx.h"
#include "BuildingMesh.h"

#include "rendering/RenderStats.h"
#include "Random.h"

#include <Vorb/graphics/GLProgram.h>

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

void BuildingMesh::addTriangle(TriangleVertex verts[3]) {
    const ui32 i = mIndexData.size();
    const ui32 v = mVertexData.size();
    mIndexData.resize(i + 3u);
    mVertexData.resize(v + 3u);
    mIndexData[i] = v;
    mIndexData[i + 1] = v + 1;
    mIndexData[i + 2] = v + 2;
    memcpy(&mVertexData[mVertexData.size() - 3], verts, sizeof(TriangleVertex) * 3);
}

void BuildingMesh::addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, CubeFacing axis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal)
{
    const ui32 v = mVertexData.size();
    mVertexData.resize(v + 4u);
    assert(!mVertexData.empty());
    TriangleVertex* verts = &mVertexData.back() - 3;

    const ui32 ind = mIndexData.size();
    mIndexData.resize(ind + 6u);
    mIndexData[ind] = v;
    mIndexData[ind + 1] = v + 1;
    mIndexData[ind + 2] = v + 2;
    mIndexData[ind + 3] = v + 2;
    mIndexData[ind + 4] = v + 3;
    mIndexData[ind + 5] = v;


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
        TriangleVertex& vbl = verts[0];
        vbl.pos = tilePosition;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = color;
        vbl.atlasPage = spriteAtlasPage;
        vbl.normal = normal;
        vbl.tangent = tangent;
    }
    { // Bottom Right
        TriangleVertex& vbr = verts[1];
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
        TriangleVertex& vtr = verts[2];
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
        TriangleVertex& vtl = verts[3];
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

void BuildingMesh::draw(const vg::GLProgram& program) const
{
    // Make sure we have been initialized
    assert(mVao);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);
    checkGlError("BuildingMesh::draw2");
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    checkGlError("BuildingMesh::draw3");
    glDrawElements(GL_TRIANGLES, (GLsizei)mIndexCount, GL_UNSIGNED_INT, 0 /* first */);
    checkGlError("BuildingMesh::draw4");
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
    checkGlError("BuildingMesh::draw");
}

void BuildingMesh::finishMesh(MeshDrawMode drawMode)
{

    if (mVertexData.size()) {
        lazyInitBuffers();
        if (mIbo == 0) {
            glGenBuffers(1, &mIbo);
        }
        mIndexCount = mIndexData.size();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexCount * sizeof(ui32), nullptr, e_cast(drawMode));
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mIndexCount * sizeof(ui32), mIndexData.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        const unsigned bufferSizeBytes = mVertexData.size() * sizeof(TriangleVertex);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        // Orphan the buffer for speed
        glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, mVertexData.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
    std::vector<TriangleVertex>().swap(mVertexData);
    std::vector<ui32>().swap(mIndexData);
    checkGlError("BuildingMesh::finishMesh");
}

void BuildingMesh::bindVertexAttribs(const vg::GLProgram& program) const
{
    // TODO: can we not do this every time?
    if (mLastUsedProgram != program.getID()) {
        mLastUsedProgram = program.getID();

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 2, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, uvs));
        }
        if (const VGAttribute* uvTileAttribute = program.tryGetAttribute("vUVTiling")) {
            glVertexAttribPointer(*uvTileAttribute, 4, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, uvTiling));
        }
        if (const VGAttribute* atlasAttribute = program.tryGetAttribute("vAtlasPage")) {
            glVertexAttribPointer(*atlasAttribute, 1, GL_UNSIGNED_SHORT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, atlasPage));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, color));
        }
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_FLOAT, true, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, normal));
        }
        if (const VGAttribute* tangentAttribute = program.tryGetAttribute("vTangent")) {
            glVertexAttribPointer(*tangentAttribute, 2, GL_BYTE, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, tangent));
        }
    }
    checkGlError("BuildingMesh::bindVertexAttribs");
}
