#include "stdafx.h"
#include "TerrainMeshBuilder.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/MeshBuilderCommon.h"


constexpr ui32 WATER_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6;
constexpr ui32 TERRAIN_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6 + TERRAIN_MESH_WIDTH_QUADS * 4 * 6;

VGBuffer TerrainMeshBuilder::sTerrainIboUI32 = 0;


void TerrainMeshBuilder::initStaticIBO() {
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

    glCreateBuffers(1, &sTerrainIboUI32);
    glNamedBufferStorage(sTerrainIboUI32, TERRAIN_MESH_INDICES * sizeof(ui32), indices.data(), 0);
    checkGlError("TerrainMesh::initGlobalIBO");
}

void TerrainMeshBuilder::finishMeshes(Mesh& terrainMesh, Mesh& waterMesh, const f32v3& worldPos) {

    // World relative
    mBoundingSphere.center += worldPos;

    // Set bounds
    terrainMesh.setPosition(worldPos);
    waterMesh.setPosition(worldPos);
    terrainMesh.setBoundingSphere(mBoundingSphere);
    waterMesh.setBoundingSphere(mBoundingSphere);

    MeshBuilderCommon::initMeshBuffers(terrainMesh.mGpuData, &sTerrainIboUI32, 0);
    MeshBuilderCommon::initMeshBuffers(waterMesh.mGpuData, &sTerrainIboUI32, 0);

    terrainMesh.mGpuData.mIndexType = MeshIndexType::INT;
    waterMesh.mGpuData.mIndexType = MeshIndexType::INT;
    terrainMesh.mGpuData.mLODData.mTotalIndexCount = TERRAIN_MESH_INDICES;
    waterMesh.mGpuData.mLODData.mTotalIndexCount = WATER_MESH_INDICES;

    MeshBuilderCommon::uploadVertexData(terrainMesh.mGpuData, mTerrainVerts, TERRAIN_MESH_SIZE_VERTS, sizeof(TerrainVertex), 0);
    MeshBuilderCommon::uploadVertexData(waterMesh.mGpuData, mWaterVerts, WATER_MESH_SIZE_VERTS, sizeof(WaterVertex), 0);

    terrainMesh.mGpuData.mVertexType = TerrainVertex::bindVertexAttribs(terrainMesh.mGpuData.mVao);
    waterMesh.mGpuData.mVertexType = WaterVertex::bindVertexAttribs(waterMesh.mGpuData.mVao);


    checkGlError("TerrainMeshBuilder::finishMeshes");
}

void TerrainMeshBuilder::setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {

    // AABB calculation
    f32AABB3 aabb;
    aabb.dims.x = totalWidth;
    aabb.dims.y = totalWidth;
    aabb.pos.x = cornerPos.x;
    aabb.pos.y = cornerPos.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = -FLT_MAX;

    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;

    constexpr f32 NORMAL_STRENGTH = 1.0f / 4.0f;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            TerrainVertex& v = mTerrainVerts[y * TERRAIN_MESH_WIDTH_VERTS + x];
            f32 height = paddedHeightfield[y + 1][x + 1];

            if (height < minZ) minZ = height;
            if (height > maxZ) maxZ = height;

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
        TerrainVertex& v = mTerrainVerts[index++];
        // Copy the vertices from the top edge
        v = mTerrainVerts[i];
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Left Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = mTerrainVerts[index++];
        // Copy the vertices from the left edge
        v = mTerrainVerts[i * TERRAIN_MESH_WIDTH_VERTS];
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Right Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = mTerrainVerts[index++];
        // Copy the vertices from the right edge
        v = mTerrainVerts[i * TERRAIN_MESH_WIDTH_VERTS + TERRAIN_MESH_WIDTH_VERTS - 1];
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Bottom Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = mTerrainVerts[index++];
        // Copy the vertices from the bottom edge
        v = mTerrainVerts[TERRAIN_MESH_WIDTH_VERTS_SQ - TERRAIN_MESH_WIDTH_VERTS + i];
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }

    // Bounding sphere
    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    mBoundingSphere = boundingSphereFromAABB(aabb);
}

void TerrainMeshBuilder::setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {

    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;

    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = mWaterVerts[y * TERRAIN_MESH_WIDTH_VERTS + x];
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
            v.pos.z = 0.0f;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.depth = glm::max(-height, 0.0f);
        }
    }
}