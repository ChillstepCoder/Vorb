#include "stdafx.h"
#include "ProceduralMeshBuilder.h"

#include <boost/pool/singleton_pool.hpp>
#include "math/Random.h"

struct mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_builder_pool, sizeof(ProceduralMeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

VGBuffer ProceduralMeshBuilder::sQuadIboUI32 = 0;

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


ProceduralMeshBuilder::ProceduralMeshBuilder(bool useSharedIndexBuffer) : mUsingSharedIndexBuffer(useSharedIndexBuffer) {
}

ProceduralMeshBuilder::~ProceduralMeshBuilder() {

}

void ProceduralMeshBuilder::addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis, const MaterialData& materialData, const f32v4& uvRect, color4 color) {
    
    SubMeshBufferData& submesh = mSubMeshesData[e_cast(materialData.renderPass)];

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh.mVerts.size();
        std::vector<ui32>& indexData = submesh.mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<StaticModelVertex>& vertexData = submesh.mVerts;
    vertexData.resize(vertexData.size() + 4);

    StaticModelVertex* verts = (StaticModelVertex*)(&vertexData.back() - 3);

    const i32v2& xyAxis = CUBE_FACING_AXIS[e_cast(axis)];
    const f32v3& normal(CUBE_FACING_NORMALSF[e_cast(axis)]);
    const f32v3& tangent(CUBE_FACING_TANGENTSF[e_cast(axis)]);
    const f32v2& xyAxisDirection = CUBE_FACING_AXIS_DIRECTIONS[e_cast(axis)];
    const f32v2& initialOffsetMult = CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(axis)];
    f32v4 uvs = uvRect;
    
    // TODO: Support indexes?
    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Offset for back faces so we can invert direction and have proper back face culling
    tilePosition[xyAxis.x] += xyDims.x * initialOffsetMult.x;
    tilePosition[xyAxis.y] += xyDims.y * initialOffsetMult.y;

    { // Bottom Left
        verts[0].build(tilePosition, normal, tangent, uvs, color, materialData.id, 0);
    }
    { // Bottom Right
        f32v3 pos = tilePosition;
        pos[xyAxis.x] += (xyDims.x) * xyAxisDirection.x;

        verts[1].build(pos, normal, tangent, f32v2(uvs.x + uvs.z, uvs.y), color, materialData.id, 0);
    }
    { // Top Right
        f32v3 pos = tilePosition;
        pos[xyAxis.x] += (xyDims.x) * xyAxisDirection.x;
        pos[xyAxis.y] += (xyDims.y) * xyAxisDirection.y;

        verts[2].build(pos, normal, tangent, f32v2(uvs.x + uvs.z, uvs.y + uvs.w), color, materialData.id, 0);
    }
    { // Top Left
        f32v3 pos = tilePosition;
        pos[xyAxis.y] += (xyDims.y) * xyAxisDirection.y;
        verts[3].build(pos, normal, tangent, f32v2(uvs.x, uvs.y + uvs.w), color, materialData.id, 0);
    }

    // HACK fix bottom faces to be correct winding
    if (axis == CubeFacing::BOTTOM) {
        std::swap(verts[0], verts[1]);
        std::swap(verts[2], verts[3]);
    }
}

void ProceduralMeshBuilder::addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], const MaterialData& materialData, color4 color, bool flipTriangleDir)
{
    constexpr f32 EPSILON = 0.01f;
    SubMeshBufferData& submesh = mSubMeshesData[e_cast(materialData.renderPass)];

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh.mVerts.size();
        std::vector<ui32>& indexData = submesh.mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<StaticModelVertex>& vertexData = submesh.mVerts;
    vertexData.resize(vertexData.size() + 4);

    StaticModelVertex* verts = (StaticModelVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // TODO: This is a bad approximation
    const f32 dX = ((terrainCorners[0] - terrainCorners[1]) + (terrainCorners[2] - terrainCorners[3])) * 0.5f;
    const f32 dY = ((terrainCorners[0] - terrainCorners[2]) + (terrainCorners[1] - terrainCorners[3])) * 0.5f;
    const f32 dZ = 1.0f;
    const f32v4 uvs(0.0f, 0.0f, 1.0f, 1.0f);

    const f32v3 normal(glm::normalize(f32v3(dX, dY, dZ)));
    const f32v3 tangent(0, 1, 0);

    if (flipTriangleDir) {
        { // Bottom Right
            verts[0].build(f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON), normal, tangent, f32v2(uvs.x + uvs.z, uvs.y), color, materialData.id, 0);
        }
        { // Top Right
            verts[1].build(f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON), normal, tangent, f32v2(uvs.x + uvs.z, uvs.y + uvs.w), color, materialData.id, 0);
        }
        { // Top Left
            verts[2].build(f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON), normal, tangent, f32v2(uvs.x, uvs.y + uvs.w), color, materialData.id, 0);
        }
        { // Bottom Left
            verts[3].build(f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON), normal, tangent, f32v2(uvs.x, uvs.y), color, materialData.id, 0);
        }
    }
    else {

        { // Bottom Left
            verts[0].build(f32v3(tilePosition.x, tilePosition.y, terrainCorners[0] + EPSILON), normal, tangent, f32v2(uvs.x, uvs.y), color, materialData.id, 0);
        }
        { // Bottom Right
            verts[1].build(f32v3(tilePosition.x + 1.0f, tilePosition.y, terrainCorners[1] + EPSILON), normal, tangent, f32v2(uvs.x + uvs.z, uvs.y), color, materialData.id, 0);
        }
        { // Top Right
            verts[2].build(f32v3(tilePosition.x + 1.0f, tilePosition.y + 1.0f, terrainCorners[3] + EPSILON), normal, tangent, f32v2(uvs.x + uvs.z, uvs.y + uvs.w), color, materialData.id, 0);
        }
        { // Top Left
            verts[3].build(f32v3(tilePosition.x, tilePosition.y + 1.0f, terrainCorners[2] + EPSILON), normal, tangent, f32v2(uvs.x, uvs.y + uvs.w), color, materialData.id, 0);
        }
    }
}

void ProceduralMeshBuilder::addTriangle(StaticModelVertex verts[3], const MaterialData& materialData, bool calculateNormals) {

    assert(!calculateNormals); // Unsupported so far
    assert(!mUsingSharedIndexBuffer); // Non shared IBO only

    SubMeshBufferData& submesh = mSubMeshesData[e_cast(materialData.renderPass)];

    mPolyTypeFlags.setBit(PolyTypeFlags::INDEXED_TRIANGLES);

    std::vector<StaticModelVertex>& vertexData = submesh.mVerts;
    std::vector<ui32>& indexData = submesh.mIndices;

    const size_t i = indexData.size();
    const size_t v = vertexData.size();
    indexData.resize(i + 3u);
    vertexData.resize(v + 3u);
    memcpy(&vertexData[vertexData.size() - 3], verts, sizeof(StaticModelVertex) * 3);
    // Set indices
    for (ui32 j = v; j < vertexData.size(); ++j) {
        vertexData[j].materialId = materialData.id;
    }
    // TODO: Can we just draw arrays this?
    indexData[i] = v;
    indexData[i + 1u] = v + 1u;
    indexData[i + 2u] = v + 2u;
}

void ProceduralMeshBuilder::addQuadBetweenPoints(const f32v3 vertPoints[4], const MaterialData& materialData, f32v2 uvScale, color4 color) {
    addQuadBetweenPoints(vertPoints[0], vertPoints[1], vertPoints[2], vertPoints[3], materialData, uvScale, color);
}

void ProceduralMeshBuilder::addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const MaterialData& materialData, f32v2 uvScale, color4 color) {
   
    SubMeshBufferData& submesh = mSubMeshesData[e_cast(materialData.renderPass)];

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh.mVerts.size();
        std::vector<ui32>& indexData = submesh.mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<StaticModelVertex>& vertexData = submesh.mVerts;
    vertexData.resize(vertexData.size() + 4);

    StaticModelVertex* verts = (StaticModelVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Compute tangents and normals
    f32v3 tangent = glm::normalize(v1 - v0);
    f32v3 normal = glm::normalize(glm::cross(tangent, v3 - v0));
    if (tangent == f32v3(0)) {
        tangent.x = 1.0f;
    }

    // TODO: These UV calculations are only accurate for perfect quads, for squished quads it wont work
    { // Bottom Left
        verts[0].build(v0, normal, tangent, f32v2(0.0f), color, materialData.id, 0);
    }
    { // Bottom Right
        verts[1].build(v1, normal, tangent, f32v2(uvScale.x * glm::length(v1 - v0), 0.0f), color, materialData.id, 0);
    }
    { // Top Right
        verts[2].build(v2, normal, tangent, f32v2(uvScale.x * glm::length(v2 - v3), uvScale.y * glm::length(v2 - v1)), color, materialData.id, 0);
    }
    { // Top Left
        verts[3].build(v3, normal, tangent, f32v2(0.0f, uvScale.y * uvScale.y * glm::length(v3 - v0)), color, materialData.id, 0);
    }
}

void ProceduralMeshBuilder::addQuadBetweenPointsWorldUV(const f32v3 vertPoints[4], const MaterialData& materialData, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv /*= false*/) {
    addQuadBetweenPointsWorldUV(vertPoints[0], vertPoints[1], vertPoints[2], vertPoints[3], materialData, uvScale, color, uvOrient, worldUVRoot, flipUv);
}

void ProceduralMeshBuilder::addQuadBetweenPointsWorldUV(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const MaterialData& materialData, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv /*= false*/) {
    
    SubMeshBufferData& submesh = mSubMeshesData[e_cast(materialData.renderPass)];

    // Add indices if needed
    if (!mUsingSharedIndexBuffer) {
        const size_t v = submesh.mVerts.size();
        std::vector<ui32>& indexData = submesh.mIndices;
        const size_t ind = indexData.size();
        indexData.resize(ind + 6u);
        indexData[ind] = v;
        indexData[ind + 1u] = v + 1u;
        indexData[ind + 2u] = v + 2u;
        indexData[ind + 3u] = v + 2u;
        indexData[ind + 4u] = v + 3u;
        indexData[ind + 5u] = v;
    }

    std::vector<StaticModelVertex>& vertexData = submesh.mVerts;
    vertexData.resize(vertexData.size() + 4);

    StaticModelVertex* verts = (StaticModelVertex*)(&vertexData.back() - 3);

    mPolyTypeFlags.setBit(PolyTypeFlags::QUADS);

    // Compute tangents and normals
    f32v3 tangent = glm::normalize(v1 - v0);
    f32v3 normal = glm::normalize(glm::cross(tangent, v3 - v0));
    if (tangent == f32v3(0)) {
        tangent.x = 1.0f;
    }

    f32v2 uvs[4];
    // TODO >:C
    const f32v3 pos[4] = { v0, v1, v2, v3 };

    // TODO: This could be an axis lookup for branchless
    switch (uvOrient) {
        case AXIS_X:
            for (int i = 0; i < 4; ++i) {
                uvs[i].x = (pos[i].y - worldUVRoot.y) * uvScale.x;
                uvs[i].y = (pos[i].z - worldUVRoot.z) * uvScale.y;
            }
            break;
        case AXIS_Y:
            for (int i = 0; i < 4; ++i) {
                uvs[i].x = (pos[i].x - worldUVRoot.x) * uvScale.x;
                uvs[i].y = (pos[i].z - worldUVRoot.z) * uvScale.y;
            }
            break;
        case AXIS_Z:
            for (int i = 0; i < 4; ++i) {
                uvs[i].x = (pos[i].x - worldUVRoot.x) * uvScale.x;
                uvs[i].y = (pos[i].y - worldUVRoot.y) * uvScale.y;
            }
            break;
        default:
            assert(false);
            break;

    }

    if (flipUv) {
        for (int i = 0; i < 4; ++i) {
            std::swap(uvs[i].x, uvs[i].y);
        }
    }

    { // Bottom Left
        verts[0].build(v0, normal, tangent, uvs[0], color, materialData.id, 0);
    }
    { // Bottom Right
        verts[1].build(v1, normal, tangent, uvs[1], color, materialData.id, 0);
    }
    { // Top Right
        verts[2].build(v2, normal, tangent, uvs[2], color, materialData.id, 0);
    }
    { // Top Left
        verts[3].build(v3, normal, tangent, uvs[3], color, materialData.id, 0);
    }

}

void ProceduralMeshBuilder::addBoardBetweenPoints(const f32v3& p1, const f32v3& p2, const f32v2& halfDims, const MaterialData& materialData, f32v2 uvScale) {
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
    addQuadBetweenPoints(pointsP1, materialData, uvScale2, COLOR_WHITE);
    // P2 cap
    addQuadBetweenPoints(pointsP2, materialData, uvScale2, COLOR_WHITE);

    // Bottom
    addQuadBetweenPoints(pointsP1[1], pointsP1[0], pointsP2[1], pointsP2[0], materialData, uvScale2, COLOR_WHITE);
    // Left
    addQuadBetweenPoints(pointsP1[2], pointsP1[1], pointsP2[0], pointsP2[3], materialData, uvScale2, COLOR_WHITE);
    // Right
    addQuadBetweenPoints(pointsP1[0], pointsP1[3], pointsP2[2], pointsP2[1], materialData, uvScale2, COLOR_WHITE);
    // Top
    addQuadBetweenPoints(pointsP1[3], pointsP1[2], pointsP2[3], pointsP2[2], materialData, uvScale2, COLOR_WHITE);

}

// This also computes an AABB but we dont store it
void ProceduralMeshBuilder::computeBoundingSphere() {
    PROFILE_FUNCTION();
    mDidComputeBoundingSphere = true;

    f32v2 minMax[3] = { {FLT_MAX, FLT_MIN}, {FLT_MAX, FLT_MIN}, {FLT_MAX, FLT_MIN} };

    for (auto& subMesh : mSubMeshesData) {
        for (auto& v : subMesh.mVerts) {
            const f32v3& pos = v.pos;
            for (int i = 0; i < 3; ++i) {
                if (pos[i] < minMax[i].x) {
                    minMax[i].x = pos[i];
                }
                if (pos[i] > minMax[i].y) {
                    minMax[i].y = pos[i];
                }
            }
        }
    }
    mBoundingSphere.center = f32v3((minMax[0].x + minMax[0].y) * 0.5f, (minMax[1].x + minMax[1].y) * 0.5f, (minMax[2].x + minMax[2].y) * 0.5f);
    const f32 halfLargestWidth = 0.5f * std::max(std::max(minMax[0].y - minMax[0].x, minMax[1].y - minMax[1].x), minMax[2].y - minMax[1].y);
    const f32 halfLargestWidthSq = SQ(halfLargestWidth);
    // TODO: This isnt quite accurate it assumes a cube instead of a rectangle, but ehh good enough for now
    mBoundingSphere.radius = sqrt(halfLargestWidthSq + halfLargestWidthSq);
}

void ProceduralMeshBuilder::finishMesh(std::unique_ptr<Mesh>& mesh, const f32v3& worldPos) {
    assert(IS_RENDER_THREAD());

    assert(mSubMeshesData[e_cast(MaterialRenderPassType::Smudge)].mVerts.empty()); // TODO: SUPPORT THIS, we should return multiple meshes

    // return blank mesh if we have no geometry
    if (mSubMeshesData[e_cast(MaterialRenderPassType::Default)].mVerts.empty()) {
        mesh.reset();
        return;
    }

    // Allocate if needed
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }

    finishMesh(*mesh,  worldPos);
}

void ProceduralMeshBuilder::finishMesh(Mesh& mesh, const f32v3& worldPos) {
    assert(IS_RENDER_THREAD());
    assert(mSubMeshesData[e_cast(MaterialRenderPassType::Smudge)].mVerts.empty()); // TODO: SUPPORT THIS, we should return multiple meshes

    // Set bounds
    mesh.mPosition = worldPos;
    mesh.mBoundingSphere = mBoundingSphere;
    if (mDidComputeBoundingSphere) {
        mesh.mBoundingSphere.center += worldPos;
    }

    // Hook in shared IBOs if needed
    bool usingSharedIbo = false;
    const ui8 polyTypeBits = mPolyTypeFlags.getBits();
    VGBuffer* sharedIbo = nullptr;
    if (polyTypeBits == e_cast(PolyTypeFlags::QUADS)) {
        usingSharedIbo = true;
        sharedIbo = &sQuadIboUI32;
    }
    static_assert(e_cast(PolyTypeFlags::COUNT) == 3, "Update any new shared IBO");

    // Make sure we didn't fuck up and say shared when it wasn't
    assert((usingSharedIbo == mUsingSharedIndexBuffer || !mUsingSharedIndexBuffer) && "Mesh was flagged improperly as shared index buffer");

    MeshBuilderCommon::initMeshBuffers(mesh.mMainMesh, sharedIbo, 0);

    uploadMeshData(mesh.mMainMesh, worldPos, mSubMeshesData[0], GL_DYNAMIC_STORAGE_BIT);

    // TODO: Support other index formats
    mesh.mMainMesh.mIndexType = MeshIndexType::INT;

}

void* ProceduralMeshBuilder::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void ProceduralMeshBuilder::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

void ProceduralMeshBuilder::uploadMeshData(MeshGpuData& subMesh, const f32v3& position, const SubMeshBufferData& data, GLbitfield flags) {

    // Shared IBO
    if (subMesh.mIbo == sQuadIboUI32) {
        subMesh.mLODData.mTotalIndexCount = (data.mVerts.size() / 4u) * 6u;
        assert(subMesh.mLODData.mTotalIndexCount < MAX_QUAD_MESH_INDICES);
    }
    else {
        // Non shared IBO
        // TODO: Support ui16 compression
        MeshBuilderCommon::uploadIndexData(subMesh, data.mIndices, flags);
    }

    MeshBuilderCommon::uploadVertexData(subMesh, data.mVerts.data(), data.mVerts.size(), sizeof(StaticModelVertex), flags);

    subMesh.mVertexType = StaticModelVertex::bindVertexAttribs(subMesh.mVao);

    checkGlError("MeshBuilder::uploadMeshData");
}


void ProceduralMeshBuilder::initStaticIBOs() {

    // ========================================
    // =              QUADS                   =
    // ========================================
    if (sQuadIboUI32) {
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

    glCreateBuffers(1, &sQuadIboUI32);
    glNamedBufferStorage(sQuadIboUI32, MAX_QUAD_MESH_INDICES * sizeof(ui32), quadIndices.data(), 0);

}

void ProceduralMeshBuilder::reserveVertexCount(ui32 count) {
    // TODO: Support smudge too?
    mSubMeshesData[e_cast(MaterialRenderPassType::Default)].mVerts.reserve(count);
}

void ProceduralMeshBuilder::reserveIndexCount(ui32 count) {
    // TODO: Support smudge too?
    mSubMeshesData[e_cast(MaterialRenderPassType::Default)].mIndices.reserve(count);
}
