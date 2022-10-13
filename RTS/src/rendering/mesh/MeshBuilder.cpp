#include "stdafx.h"
#include "MeshBuilder.h"

#include "rendering/texture/SubTexture.h"

#include <boost/pool/singleton_pool.hpp>
#include "math/Random.h"

#define SUBMESH_INDEX_MAIN -1

constexpr ui32 MAX_TEXTURES_PER_MESH = 500; // Must be even
constexpr ui32 WATER_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6;
constexpr ui32 TERRAIN_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6 + TERRAIN_MESH_WIDTH_QUADS * 4 * 6;

struct mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_builder_pool, sizeof(MeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

VGBuffer MeshBuilder::sQuadIbo = 0;
VGBuffer MeshBuilder::sTerrainIbo = 0;

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


MeshBuilder::MeshBuilder(bool useSharedIndexBuffer) : mUsingSharedIndexBuffer(useSharedIndexBuffer) {

}

MeshBuilder::~MeshBuilder() {

}


void MeshBuilder::setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {
    assert(mUsingSharedIndexBuffer); // Shared only
    
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    std::vector<Vertex32>& terrainVerts = mMainSubMeshData.mVerts;
    terrainVerts.resize(TERRAIN_MESH_SIZE_VERTS);

    // We are a terrain mesh
    mPolyTypeFlags.setBit(PolyTypeFlags::TERRAIN);
    
    constexpr f32 NORMAL_STRENGTH = 1.0f / 4.0f;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            TerrainVertex& v = terrainVerts[y * TERRAIN_MESH_WIDTH_VERTS + x].mTerrain;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
            v.pos.z = height;

            // Normal calc
            f32 fl = paddedHeightfield[y][x]; // front left
            f32  l = paddedHeightfield[y + 1][x];   // left
            f32 bl = paddedHeightfield[y + 2][x]; // back left
            f32  f = paddedHeightfield[y][x + 1];   // front
            f32  b = paddedHeightfield[y + 2][x + 1];   // back
            f32 fr = paddedHeightfield[y][x + 2]; // front right
            f32  r = paddedHeightfield[y + 1][x + 2];   // right
            f32 br = paddedHeightfield[y + 2][x + 2]; // back right

            //https://gamedev.stackexchange.com/questions/165575/calculating-normal-map-from-height-map-using-sobel-operator
            // Sobel filter
            const f32 dX = (fl + 2.0f * l + bl) - (fr + 2.0f * r + br);
            const f32 dY = (fl + 2.0f * f + fr) - (bl + 2.0f * b + br);
            const f32 dZ = quadWidth;

            f32v3 n(dX, dY, dZ);
            v.normal = glm::normalize(n);
        }
    }

    ui32 index = SQ(TERRAIN_MESH_WIDTH_VERTS);
    const float SKIRT_DEPTH = quadWidth * 2.0f;
    // Build skirts
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the top edge
        v = terrainVerts[i].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Left Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the left edge
        v = terrainVerts[i * TERRAIN_MESH_WIDTH_VERTS].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Right Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the right edge
        v = terrainVerts[i * TERRAIN_MESH_WIDTH_VERTS + TERRAIN_MESH_WIDTH_VERTS - 1].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Bottom Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the bottom edge
        v = terrainVerts[TERRAIN_MESH_WIDTH_VERTS_SQ - TERRAIN_MESH_WIDTH_VERTS + i].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
}

void MeshBuilder::setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS])
{
    assert(mUsingSharedIndexBuffer); // Shared only

    std::vector<Vertex32>& waterVerts = mMainSubMeshData.mVerts;
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    waterVerts.resize(TERRAIN_MESH_SIZE_VERTS);
    mPolyTypeFlags.setBit(PolyTypeFlags::WATER);

    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = waterVerts[y * TERRAIN_MESH_WIDTH_VERTS + x].mWater;
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
            v.pos.z = 0.0f;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.depth = glm::max(-height, 0.0f);
        }
    }
}

void MeshBuilder::addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis, const SubTexture& texture, const f32v4& uvRect, color4 color) {
    
    InProgressSubMeshData* submesh;
    ui8 textureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &textureIndex);

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh->mVerts.size();
        std::vector<ui32>& indexData = submesh->mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<Vertex32>& vertexData = submesh->mVerts;
    vertexData.resize(vertexData.size() + 4);

    StandardVertex* verts = (StandardVertex*)(&vertexData.back() - 3);

    const i32v2& xyAxis = CUBE_FACING_AXIS[e_cast(axis)];
    const i8v3 normal(CUBE_FACING_NORMALS[e_cast(axis)]);
    const i8v2 tangent(CUBE_FACING_TANGENTS[e_cast(axis)]);
    const f32v2& xyAxisDirection = CUBE_FACING_AXIS_DIRECTIONS[e_cast(axis)];
    const f32v2& initialOffsetMult = CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(axis)];
    f32v4 uvs = uvRect;
    
    // TODO: Support indexes?
    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Offset for back faces so we can invert direction and have proper back face culling
    tilePosition[xyAxis.x] += xyDims.x * initialOffsetMult.x;
    tilePosition[xyAxis.y] += xyDims.y * initialOffsetMult.y;

    if (texture.mFlags.isBitSet(SubTextureFlags::RAND_FLIP) && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        uvs.x = uvs.x + uvs.z;
        uvs.y = uvs.y;
        uvs.z = -uvs.z;
        uvs.w = uvs.w;
    }

    { // Bottom Left
        StandardVertex& vbl = verts[0];
        vbl.pos = tilePosition;
        vbl.uvs.x = uvs.x;
        vbl.uvs.y = uvs.y + uvs.w;
        vbl.color = color;
        vbl.textureIndex = textureIndex;
        vbl.normal = normal;
        vbl.tangent = tangent;
    }
    { // Bottom Right
        StandardVertex& vbr = verts[1];
        vbr.pos = tilePosition;
        vbr.uvs.x = uvs.x + uvs.z;
        vbr.uvs.y = uvs.y + uvs.w;
        vbr.color = color;
        vbr.textureIndex = textureIndex;
        vbr.pos[xyAxis.x] += (xyDims.x) * xyAxisDirection.x;
        vbr.normal = normal;
        vbr.tangent = tangent;
    }
    { // Top Right
        StandardVertex& vtr = verts[2];
        vtr.pos = tilePosition;
        vtr.uvs.x = uvs.x + uvs.z;
        vtr.uvs.y = uvs.y;
        vtr.color = color;
        vtr.textureIndex = textureIndex;
        vtr.pos[xyAxis.x] += (xyDims.x) * xyAxisDirection.x;
        vtr.pos[xyAxis.y] += (xyDims.y) * xyAxisDirection.y;
        vtr.normal = normal;
        vtr.tangent = tangent;
    }
    { // Top Left
        StandardVertex& vtl = verts[3];
        vtl.pos = tilePosition;
        vtl.uvs.x = uvs.x;
        vtl.uvs.y = uvs.y;
        vtl.color = color;
        vtl.textureIndex = textureIndex;
        vtl.pos[xyAxis.y] += (xyDims.y) * xyAxisDirection.y;
        vtl.normal = normal;
        vtl.tangent = tangent;
    }

    // HACK fix bottom faces to be correct winding
    if (axis == CubeFacing::BOTTOM) {
        std::swap(verts[0], verts[1]);
        std::swap(verts[2], verts[3]);
    }
}

void MeshBuilder::addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], const SubTexture& texture, color4 color, bool flipTriangleDir)
{
    constexpr f32 EPSILON = 0.01f;
    InProgressSubMeshData* submesh;
    ui8 textureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &textureIndex);

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh->mVerts.size();
        std::vector<ui32>& indexData = submesh->mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<Vertex32>& vertexData = submesh->mVerts;
    vertexData.resize(vertexData.size() + 4);

    StandardVertex* verts = (StandardVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // TODO: This is a bad approximation
    const f32 dX = ((terrainCorners[0] - terrainCorners[1]) + (terrainCorners[2] - terrainCorners[3])) * 0.5f;
    const f32 dY = ((terrainCorners[0] - terrainCorners[2]) + (terrainCorners[1] - terrainCorners[3])) * 0.5f;
    const f32 dZ = 1.0f;
    f32v4 uvs = texture.mUvRect;

    f32v3 n(dX, dY, dZ);
    i8v3 normal(glm::normalize(n) * 127.0f);

    const i8v2 tangent(0, 1);

    if (texture.mFlags.isBitSet(SubTextureFlags::RAND_FLIP) && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        uvs.x = uvs.x;
        uvs.y = uvs.y;
        uvs.z = -uvs.z;
        uvs.w = uvs.w;
    }

    if (flipTriangleDir) {
        { // Bottom Right
            StandardVertex& vbr = verts[0];
            vbr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON);
            vbr.uvs.x = uvs.x + uvs.z;
            vbr.uvs.y = uvs.y + uvs.w;
            vbr.color = color;
            vbr.textureIndex = textureIndex;
            vbr.normal = normal;
            vbr.tangent = tangent;
        }
        { // Top Right
            StandardVertex& vtr = verts[1];
            vtr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON);
            vtr.uvs.x = uvs.x + uvs.z;
            vtr.uvs.y = uvs.y;
            vtr.color = color;
            vtr.textureIndex = textureIndex;
            vtr.normal = normal;
            vtr.tangent = tangent;
        }
        { // Top Left
            StandardVertex& vtl = verts[2];
            vtl.pos = f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON);
            vtl.uvs.x = uvs.x;
            vtl.uvs.y = uvs.y;
            vtl.color = color;
            vtl.textureIndex = textureIndex;
            vtl.normal = normal;
            vtl.tangent = tangent;
        }
        { // Bottom Left
            StandardVertex& vbl = verts[3];
            vbl.pos = f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON);
            vbl.uvs.x = uvs.x;
            vbl.uvs.y = uvs.y + uvs.w;
            vbl.color = color;
            vbl.textureIndex = textureIndex;
            vbl.normal = normal;
            vbl.tangent = tangent;
        }
    }
    else {

        { // Bottom Left
            StandardVertex& vbl = verts[0];
            vbl.pos = f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON);
            vbl.uvs.x = uvs.x;
            vbl.uvs.y = uvs.y + uvs.w;
            vbl.color = color;
            vbl.textureIndex = textureIndex;
            vbl.normal = normal;
            vbl.tangent = tangent;
        }
        { // Bottom Right
            StandardVertex& vbr = verts[1];
            vbr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON);
            vbr.uvs.x = uvs.x + uvs.z;
            vbr.uvs.y = uvs.y + uvs.w;
            vbr.color = color;
            vbr.textureIndex = textureIndex;
            vbr.normal = normal;
            vbr.tangent = tangent;
        }
        { // Top Right
            StandardVertex& vtr = verts[2];
            vtr.pos = f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON);
            vtr.uvs.x = uvs.x + uvs.z;
            vtr.uvs.y = uvs.y;
            vtr.color = color;
            vtr.textureIndex = textureIndex;
            vtr.normal = normal;
            vtr.tangent = tangent;
        }
        { // Top Left
            StandardVertex& vtl = verts[3];
            vtl.pos = f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON);
            vtl.uvs.x = uvs.x;
            vtl.uvs.y = uvs.y;
            vtl.color = color;
            vtl.textureIndex = textureIndex;
            vtl.normal = normal;
            vtl.tangent = tangent;
        }
    }
}

void MeshBuilder::addTriangle(StandardVertex verts[3], const SubTexture& texture, bool calculateNormals) {
    static_assert(sizeof(StandardVertex) == sizeof(Vertex32));

    assert(!calculateNormals); // Unsupported so far
    assert(!mUsingSharedIndexBuffer); // Non shared IBO only

    InProgressSubMeshData* submesh;
    ui8 textureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &textureIndex);

    mPolyTypeFlags.setBit(PolyTypeFlags::INDEXED_TRIANGLES);

    std::vector<Vertex32>& vertexData = submesh->mVerts;
    std::vector<ui32>& indexData = submesh->mIndices;

    const size_t i = indexData.size();
    const size_t v = vertexData.size();
    indexData.resize(i + 3u);
    vertexData.resize(v + 3u);
    memcpy(&vertexData[vertexData.size() - 3], verts, sizeof(StandardVertex) * 3);
    // Set indices
    for (ui32 j = v; j < vertexData.size(); ++j) {
        vertexData[j].mStandard.textureIndex = textureIndex;
    }
    // TODO: Can we just draw arrays this?
    indexData[i] = v;
    indexData[i + 1u] = v + 1u;
    indexData[i + 2u] = v + 2u;
}

void MeshBuilder::addQuadBetweenPoints(const f32v3 vertPoints[4], const SubTexture& texture, f32v2 uvScale, color4 color) {
    addQuadBetweenPoints(vertPoints[0], vertPoints[1], vertPoints[2], vertPoints[3], texture, uvScale, color);
}

void MeshBuilder::addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const SubTexture& texture, f32v2 uvScale, color4 color) {
    InProgressSubMeshData* submesh;
    ui8 textureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &textureIndex);

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh->mVerts.size();
        std::vector<ui32>& indexData = submesh->mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<Vertex32>& vertexData = submesh->mVerts;
    vertexData.resize(vertexData.size() + 4);

    StandardVertex* verts = (StandardVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Compute tangents and normals
    f32v3 tangentF = glm::normalize(v1 - v0);
    f32v3 normalf = glm::normalize(glm::cross(tangentF, v3 - v0));

    const i8v3 normal = compressNormal(normalf);
    i8v3 tangent3 = compressNormal(tangentF);
    i8v2 tangent(tangent3.x, tangent3.y);
    if (tangent == i8v2(0)) {
        tangent.x = 1.0f;
    }

    // TODO: These UV calculations are only accurate for perfect quads, for squished quads it wont work
    { // Bottom Left
        StandardVertex& vbl = verts[0];
        vbl.pos = v0;
        vbl.uvs.x = 0.0f;
        vbl.uvs.y = 0.0f;
        vbl.color = color;
        vbl.textureIndex = textureIndex;
        vbl.normal = normal;
        vbl.tangent = tangent;
    }
    { // Bottom Right
        StandardVertex& vbr = verts[1];
        vbr.pos = v1;
        vbr.uvs.x = uvScale.x * glm::length(v1 - v0);
        vbr.uvs.y = 0.0f;
        vbr.color = color;
        vbr.textureIndex = textureIndex;
        vbr.normal = normal;
        vbr.tangent = tangent;
    }
    { // Top Right
        StandardVertex& vtr = verts[2];
        vtr.pos = v2;
        vtr.uvs.x = uvScale.x * glm::length(v2 - v3);
        vtr.uvs.y = uvScale.y * glm::length(v2 - v1);
        vtr.color = color;
        vtr.textureIndex = textureIndex;
        vtr.normal = normal;
        vtr.tangent = tangent;
    }
    { // Top Left
        StandardVertex& vtl = verts[3];
        vtl.pos = v3;
        vtl.uvs.x = uvScale.x * 0.0f;
        vtl.uvs.y = uvScale.y * glm::length(v3 - v0);
        vtl.color = color;
        vtl.textureIndex = textureIndex;
        vtl.normal = normal;
        vtl.tangent = tangent;
    }
}

void MeshBuilder::addQuadBetweenPointsWorldUV(const f32v3 vertPoints[4], const SubTexture& texture, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv /*= false*/) {
    addQuadBetweenPointsWorldUV(vertPoints[0], vertPoints[1], vertPoints[2], vertPoints[3], texture, uvScale, color, uvOrient, worldUVRoot, flipUv);
}

void MeshBuilder::addQuadBetweenPointsWorldUV(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const SubTexture& texture, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv /*= false*/) {
    InProgressSubMeshData* submesh;
    ui8 textureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &textureIndex);

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh->mVerts.size();
        std::vector<ui32>& indexData = submesh->mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<Vertex32>& vertexData = submesh->mVerts;
    vertexData.resize(vertexData.size() + 4);

    StandardVertex* verts = (StandardVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Compute tangents and normals
    f32v3 tangentF = glm::normalize(v1 - v0);
    f32v3 normalf = glm::normalize(glm::cross(tangentF, v3 - v0));

    const i8v3 normal = compressNormal(normalf);
    i8v3 tangent3 = compressNormal(tangentF);
    i8v2 tangent(tangent3.x, tangent3.y);
    if (tangent == i8v2(0)) {
        tangent.x = 1.0f;
    }

    
    { // Bottom Left
        StandardVertex& vbl = verts[0];
        vbl.pos = v0;
        vbl.color = color;
        vbl.textureIndex = textureIndex;
        vbl.normal = normal;
        vbl.tangent = tangent;
    }
    { // Bottom Right
        StandardVertex& vbr = verts[1];
        vbr.pos = v1;
        vbr.color = color;
        vbr.textureIndex = textureIndex;
        vbr.normal = normal;
        vbr.tangent = tangent;
    }
    { // Top Right
        StandardVertex& vtr = verts[2];
        vtr.pos = v2;
        vtr.color = color;
        vtr.textureIndex = textureIndex;
        vtr.normal = normal;
        vtr.tangent = tangent;
    }
    { // Top Left
        StandardVertex& vtl = verts[3];
        vtl.pos = v3;
        vtl.color = color;
        vtl.textureIndex = textureIndex;
        vtl.normal = normal;
        vtl.tangent = tangent;
    }

    // TODO: This could be an axis lookup for branchless
    switch (uvOrient) {
        case AXIS_X:
            for (int i = 0; i < 4; ++i) {
                verts[i].uvs.x = (verts[i].pos.y - worldUVRoot.y) * uvScale.x;
                verts[i].uvs.y = (verts[i].pos.z - worldUVRoot.z) * uvScale.y;
            }
            break;
        case AXIS_Y:
            for (int i = 0; i < 4; ++i) {
                verts[i].uvs.x = (verts[i].pos.x - worldUVRoot.x) * uvScale.x;
                verts[i].uvs.y = (verts[i].pos.z - worldUVRoot.z) * uvScale.y;
            }
            break;
        case AXIS_Z:
            for (int i = 0; i < 4; ++i) {
                verts[i].uvs.x = (verts[i].pos.x - worldUVRoot.x) * uvScale.x;
                verts[i].uvs.y = (verts[i].pos.y - worldUVRoot.y) * uvScale.y;
            }
            break;
        default:
            assert(false);
            break;

    }
    if (flipUv) {
        for (int i = 0; i < 4; ++i) {
            std::swap(verts[i].uvs.x, verts[i].uvs.y);
        }
    }
}

void MeshBuilder::addBoardBetweenPoints(const f32v3& p1, const f32v3& p2, const f32v2& halfDims, const SubTexture& texture, f32v2 uvScale) {
    f32v3 offset = p2 - p1;
    f32v3 tangent = glm::cross(offset, f32v3(0.0f, 0.0f, 1.0f));
    // If vertical board, new tangent
    if (glm::length2(tangent) < 0.00001f) {
        tangent = glm::cross(offset, f32v3(1.0f, 0.0f, 0.0f));
    }
    tangent = glm::normalize(tangent);
    f32v3 bitangent = glm::normalize(glm::cross(tangent, offset));

    f32v3 pointsP1[4];
    pointsP1[0] = p1 - tangent * halfDims.x - bitangent * halfDims.y; // BR
    pointsP1[1] = p1 + tangent * halfDims.x - bitangent * halfDims.y; // BL
    pointsP1[2] = p1 + tangent * halfDims.x + bitangent * halfDims.y; // TL
    pointsP1[3] = p1 - tangent * halfDims.x + bitangent * halfDims.y; // TR

    f32v3 pointsP2[4];
    pointsP2[0] = p2 + tangent * halfDims.x - bitangent * halfDims.y; // BL
    pointsP2[1] = p2 - tangent * halfDims.x - bitangent * halfDims.y; // BR
    pointsP2[2] = p2 - tangent * halfDims.x + bitangent * halfDims.y; // TR
    pointsP2[3] = p2 + tangent * halfDims.x + bitangent * halfDims.y; // TL

    // P1 cap
    f32v2 uvScale2(uvScale);
    addQuadBetweenPoints(pointsP1, texture, uvScale2, COLOR_WHITE);
    // P2 cap
    addQuadBetweenPoints(pointsP2, texture, uvScale2, COLOR_WHITE);

    // Bottom
    addQuadBetweenPoints(pointsP1[1], pointsP1[0], pointsP2[1], pointsP2[0], texture, uvScale2, COLOR_WHITE);
    // Left
    addQuadBetweenPoints(pointsP1[2], pointsP1[1], pointsP2[0], pointsP2[3], texture, uvScale2, COLOR_WHITE);
    // Right
    addQuadBetweenPoints(pointsP1[0], pointsP1[3], pointsP2[2], pointsP2[1], texture, uvScale2, COLOR_WHITE);
    // Top
    addQuadBetweenPoints(pointsP1[3], pointsP1[2], pointsP2[3], pointsP2[2], texture, uvScale2, COLOR_WHITE);

}

void MeshBuilder::finishMesh(Mesh& mesh, MeshDrawMode drawMode) {

    // return blank mesh if we have no geometry
    if (mMainSubMeshData.mVerts.empty()) {
        return;
    }

    // Set bounds
    mesh.mBoundingSphere = mBoundingSphere;

    // Check if we need to destroy some old submeshes
    const bool wasUsingSharedIbo = mesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO);
    if (mesh.mSubMeshes.size() > mSubMeshesData.size()) {
        for (size_t i = mSubMeshesData.size(); i < mesh.mSubMeshes.size(); ++i) {
            mesh.mSubMeshes[i].destroy(wasUsingSharedIbo);
        }
    }
    // Allocate correct number of submeshes
    mesh.mSubMeshes.resize(mSubMeshesData.size());

    const bool usingTextureUbo = mMainSubMeshData.mTextures.size() > 0u;

    // Hook in shared IBOs if needed
    bool usingSharedIbo = false;
    const ui8 polyTypeBits = mPolyTypeFlags.getBits();
    if (polyTypeBits == e_cast(PolyTypeFlags::QUADS)) {
        usingSharedIbo = true;
        setSharedIbo(mesh, wasUsingSharedIbo, sQuadIbo);
    }
    else if ((polyTypeBits == e_cast(PolyTypeFlags::TERRAIN)) || (polyTypeBits == e_cast(PolyTypeFlags::WATER))) {
        usingSharedIbo = true;
        setSharedIbo(mesh, wasUsingSharedIbo, sTerrainIbo);
    }
    else {
        // Make sure we don't have terrain mixed with something else
        assert(!mPolyTypeFlags.isBitSet(PolyTypeFlags::TERRAIN));
    }
    static_assert(e_cast(PolyTypeFlags::COUNT) == 5, "Update any new shared IBO");

    // Make sure we didn't fuck up and say shared when it wasnt
    assert((usingSharedIbo == mUsingSharedIndexBuffer || !mUsingSharedIndexBuffer) && "Mesh was flagged improperly as shared index buffer");

    // Allocate all buffers if needed
    initMeshBuffers(mesh.mMainMesh, usingTextureUbo, !usingSharedIbo);
    for (auto&& subMesh : mesh.mSubMeshes) {
        initMeshBuffers(subMesh, usingTextureUbo, !usingSharedIbo);
    }

    // Upload data
    uploadMeshData(mesh.mMainMesh, mMainSubMeshData, drawMode);
    mMainSubMeshData.clear();
    for (size_t i = 0; i < mesh.mSubMeshes.size(); ++i) {
        uploadMeshData(mesh.mSubMeshes[i], mSubMeshesData[i], drawMode);
        mSubMeshesData[i].clear();
    }

    // Cleanup
    // TODO: Do we need this really?
    mSubMeshesData.clear();
    mTextureToSubmesh.clear();
    mPolyTypeFlags.clearBits();

    glBindVertexArray(0);
}


void* MeshBuilder::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void MeshBuilder::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

void MeshBuilder::getSubmeshAndTextureIndex(const SubTexture& texture, OUT InProgressSubMeshData** submesh, OUT ui8* textureIndex) {
    auto&& it = mTextureToSubmesh.find(texture.mTextureDiffuse);
    if (it != mTextureToSubmesh.end()) {
        i32 submeshIndex = it->second.first;
        *textureIndex = it->second.second;
        if (submeshIndex == SUBMESH_INDEX_MAIN) {
            *submesh = &mMainSubMeshData;
        }
        else {
            *submesh = &mSubMeshesData[submeshIndex];
        }
    }
    else {
        if (mMainSubMeshData.mTextures.size() < MAX_TEXTURES_PER_MESH) {
            // This texture fits in the main submesh
            *textureIndex = mMainSubMeshData.mTextures.size();
            mMainSubMeshData.mTextures.emplace_back(texture.mTextureHandleDiffuse);
            mMainSubMeshData.mTextures.emplace_back(texture.mTextureHandleNormal);
            mTextureToSubmesh[texture.mTextureDiffuse] = std::make_pair(SUBMESH_INDEX_MAIN, *textureIndex);
            *submesh = &mMainSubMeshData;
        }
        else {
            // Our main mesh has too many textures already, find a valid submesh for it
            bool foundSubmesh = false;
            for (size_t i = 0; i < mSubMeshesData.size(); ++i) {
                InProgressSubMeshData& data = mSubMeshesData[i];
                if (data.mTextures.size() < MAX_TEXTURES_PER_MESH) {
                    // This texture fits in the main submesh
                    *textureIndex = data.mTextures.size();
                    data.mTextures.emplace_back(texture.mTextureHandleDiffuse);
                    data.mTextures.emplace_back(texture.mTextureHandleNormal);
                    mTextureToSubmesh[texture.mTextureDiffuse] = std::make_pair(i, *textureIndex);
                    foundSubmesh = true;
                    *submesh = &data;
                    break;
                }
            }
            // No valid submesh, make a new submesh
            if (!foundSubmesh) {
                InProgressSubMeshData& data = mSubMeshesData.emplace_back();
                *textureIndex = 0;
                data.mTextures.emplace_back(texture.mTextureHandleDiffuse);
                data.mTextures.emplace_back(texture.mTextureHandleNormal);
                mTextureToSubmesh[texture.mTextureDiffuse] = std::make_pair(mSubMeshesData.size() - 1, *textureIndex);
                *submesh = &data;
            }
        }
    }
    // Ignoring normals when applying to the mesh verts
    *textureIndex = *textureIndex / 2;
}

void MeshBuilder::setSharedIbo(Mesh& mesh, const bool wasUsingSharedIbo, VGBuffer sharedIbo) {
    // Delete old IBO if needed
    if (mesh.mMainMesh.mIbo != 0 && !wasUsingSharedIbo) {
        glDeleteBuffers(1, &mesh.mMainMesh.mIbo);
    }
    mesh.mMainMesh.mIbo = sharedIbo;
    for (auto&& subMesh : mesh.mSubMeshes) {
        // Delete old IBO if needed
        if (subMesh.mIbo != 0 && !wasUsingSharedIbo) {
            glDeleteBuffers(1, &subMesh.mIbo);
        }
        subMesh.mIbo = sharedIbo;
    }
    mesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);
}

void MeshBuilder::initMeshBuffers(SubMeshData& subMesh, bool allocateUbo, bool allocateIbo) {
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
    }
    // VBO
    glBindVertexArray(subMesh.mVao);
    if (subMesh.mVbo == 0) {
        glGenBuffers(1, &subMesh.mVbo);
    }
    // UBO
    if (allocateUbo) {
        if (subMesh.mUbo == 0) {
            glGenBuffers(1, &subMesh.mUbo);
        }
    }
    else if (subMesh.mUbo) {
        glDeleteBuffers(1, &subMesh.mUbo);
        subMesh.mUbo = 0;
    }
    // IBO
    if (allocateIbo && subMesh.mIbo == 0) {
        glGenBuffers(1, &subMesh.mIbo);
        // We dont need to delete IBO if non allocating, since
        // we will have already done so in finishMesh
    }

    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    // texture UBOs go at index 1 since globalUBO is index 0
    if (subMesh.mUbo) {
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);
    }

    checkGlError("MeshBuilder::initMeshBuffers");
}

void MeshBuilder::uploadMeshData(SubMeshData& subMesh, const InProgressSubMeshData& data, MeshDrawMode drawMode) {
    glBindVertexArray(subMesh.mVao);
    
    const size_t vertexCount = data.mVerts.size();

    // IBO
    if (subMesh.mIbo == sQuadIbo) {
        subMesh.mIndexCount = (vertexCount / 4u) * 6u;
        assert(subMesh.mIndexCount < MAX_QUAD_MESH_INDICES);
    }
    else if (subMesh.mIbo == sTerrainIbo) {
        if (mPolyTypeFlags.isBitSet(PolyTypeFlags::WATER)) {
            subMesh.mIndexCount = WATER_MESH_INDICES;
        }
        else {
            subMesh.mIndexCount = TERRAIN_MESH_INDICES;
        }
    }
    else {
        // Non shared IBO
        // TODO: Support ui16 compression
        subMesh.mIndexCount = data.mIndices.size();
        const ui32 indexBufferSizeBytes = subMesh.mIndexCount * sizeof(ui32);
        // Allocate orphaned
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, data.mIndices.data());
    }
    const unsigned bufferSizeBytes = vertexCount * sizeof(Vertex32);

    // VBO
    // Allocate orphaned
    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, data.mVerts.data());

    // UBO
    if (subMesh.mUbo) {
        const ui32 textureBufferSizeBytes = data.mTextures.size() * sizeof(TextureHandle);
        // Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
        ui32v2 buffer[MAX_TEXTURES_PER_MESH * 2];
        for (ui32 i = 0; i < data.mTextures.size(); ++i) {
            TextureHandle handle = data.mTextures[i];
            buffer[i].x = handle & 0xffffffff;
            buffer[i].y = handle >> 32;
        }
        // Allocate orphaned
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
        glBufferData(GL_UNIFORM_BUFFER, textureBufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_UNIFORM_BUFFER, 0, textureBufferSizeBytes, buffer);
    }
    checkGlError("MeshBuilder::uploadMeshData");

    bindVertexAttribs(subMesh);
}

void MeshBuilder::bindVertexAttribs(SubMeshData& subMesh)
{
    if (mPolyTypeFlags.isBitSet(PolyTypeFlags::TERRAIN)) {
        // Terrain verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
    }
    else if (mPolyTypeFlags.isBitSet(PolyTypeFlags::WATER)) {
        // Water verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 1 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, depth));
    }
    else {
        // Standard verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, uvs));
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, sizeof(StandardVertex), (void*)offsetof(StandardVertex, textureIndex));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, color));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, normal));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5 /*index*/, 2 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, tangent));
        //glVertexAttribPointer(6 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, windInfluence));
    }
}

void MeshBuilder::initStaticIBOs() {

    // ========================================
    // =              QUADS                   =
    // ========================================
    if (sQuadIbo) {
        return;
    }

    ui32 i = 0;
    std::vector<ui32> quadIndices(MAX_QUAD_MESH_INDICES);
    for (ui32 v = 0; i < MAX_QUAD_MESH_INDICES; v += 4u) {
        quadIndices[i++] = v;
        quadIndices[i++] = v + 1;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v;
    }

    glGenBuffers(1, &sQuadIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_QUAD_MESH_INDICES * sizeof(ui32), quadIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // ========================================
    // =              TERRAIN                 =
    // ========================================
    std::vector<ui32> indices(TERRAIN_MESH_INDICES);
    ui32 index = 0;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_QUADS; y++) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_QUADS; x++) {
            // Compute index of back left vertex
            ui32 vertIndex = y * TERRAIN_MESH_WIDTH_VERTS + x;
            // Change triangle orientation based on odd or even
            if ((x + y) % 2) {
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
            }
            else {
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
            }
        }
    }

    // Skirt vertices
    ui32 skirtIndex = TERRAIN_MESH_WIDTH_VERTS_SQ;
    ui32 vertIndex;
    // Top Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i;
        indices[index++] = skirtIndex;
        indices[index++] = skirtIndex + 1;
        indices[index++] = vertIndex + 1;
        indices[index++] = vertIndex + 1;
        indices[index++] = vertIndex;
        indices[index++] = skirtIndex;
        skirtIndex++;
    }
    skirtIndex++; // Skip last vertex
    // Left Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i * TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = skirtIndex;
        indices[index++] = vertIndex;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex;
        skirtIndex++;
    }
    skirtIndex++; // Skip last vertex
    // Right Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i * TERRAIN_MESH_WIDTH_VERTS + TERRAIN_MESH_WIDTH_VERTS - 1;
        indices[index++] = vertIndex;
        indices[index++] = skirtIndex;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = vertIndex;
        skirtIndex++;
    }
    skirtIndex++;
    // Bottom Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = TERRAIN_MESH_WIDTH_VERTS_SQ - TERRAIN_MESH_WIDTH_VERTS + i;
        indices[index++] = vertIndex;
        indices[index++] = vertIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex;
        indices[index++] = vertIndex;
        skirtIndex++;
    }

    assert(index == TERRAIN_MESH_INDICES);

    glGenBuffers(1, &sTerrainIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sTerrainIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, TERRAIN_MESH_INDICES * sizeof(ui32), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    checkGlError("TerrainMesh::initGlobalIBO");
}

void MeshBuilder::reserveVertexCount(ui32 count) {
    mMainSubMeshData.mVerts.reserve(count);
    mMainSubMeshData.mTextures.reserve(5); // Arbitrary
}

void MeshBuilder::reserveIndexCount(ui32 count) {
    mMainSubMeshData.mIndices.reserve(count);
}
