#include "stdafx.h"
#include "TerrainMeshBuilder.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/MeshBuilderCommon.h"

#include "terrain/HeightmapPatch.h"
#include "options/DebugOptions.h"

#include <math.h>  /* modf */

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

void TerrainMeshBuilder::finishMeshes(TerrainMesh& terrainMesh, TerrainMesh& waterMesh, const f32v3& worldPosTreeRoot) {
    ASSERT_RENDER_THREAD();

    terrainMesh.mUVRoot = mUVRoot;

    // Set bounds
    terrainMesh.setPosition(f32v3(mWorldPosPatchCorner.x, mWorldPosPatchCorner.y, 0.0f));
    waterMesh.setPosition(worldPosTreeRoot);
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

    // Upload splat texture
    if (!terrainMesh.mTerrainSurfaceDataTexture) {
        glCreateTextures(GL_TEXTURE_2D, 1, &terrainMesh.mTerrainSurfaceDataTexture);
        glTextureStorage2D(terrainMesh.mTerrainSurfaceDataTexture, 1, GL_RGBA8, TERRAIN_MESH_PADDED_WIDTH_VERTS, TERRAIN_MESH_PADDED_WIDTH_VERTS);
    }
    glTextureSubImage2D(terrainMesh.mTerrainSurfaceDataTexture, 0, 0, 0, TERRAIN_MESH_PADDED_WIDTH_VERTS, TERRAIN_MESH_PADDED_WIDTH_VERTS, GL_RGBA, GL_UNSIGNED_BYTE, mTerrainSurfaceLayers);
    vg::sSamplerStates.POINT_CLAMP.setForTexture(terrainMesh.mTerrainSurfaceDataTexture);

    checkGlError("TerrainMeshBuilder::finishMeshes");
}

void TerrainMeshBuilder::setVertsTerrainFromPaddedHeightfield(
    i32v2 worldPosTreeRoot,
    i32v2 cornerPosRelativeToRoot,
    f32 totalWidth,
    const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS],
    const RoadGrid& roadGrid) {

    mWorldPosPatchCorner = worldPosTreeRoot + cornerPosRelativeToRoot;

    // AABB calculation
    f32AABB3 aabb;
    aabb.dims.x = totalWidth;
    aabb.dims.y = totalWidth;
    aabb.pos.x = mWorldPosPatchCorner.x;
    aabb.pos.y = mWorldPosPatchCorner.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = -FLT_MAX;

    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;

    { // Compute with high precision to avoid precision issues in shader
        f64v2 rootUVDouble = f64v2(mWorldPosPatchCorner) * f64(sDebugOptions.mGrassColorMapScale);
        f64 intpart;
        mUVRoot.x = (f32)modf(rootUVDouble.x, &intpart);
        mUVRoot.y = (f32)modf(rootUVDouble.y, &intpart);
    }

    constexpr f32 NORMAL_STRENGTH = 1.0f / 4.0f;
    i32v2 worldPos;
    for (i32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        worldPos.y = mWorldPosPatchCorner.y + y * quadWidth;
        for (i32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            worldPos.x = mWorldPosPatchCorner.x + x * quadWidth;
            TerrainVertex& v = mTerrainVerts[y * TERRAIN_MESH_WIDTH_VERTS + x];
            f32 height = uncompressHeight(paddedHeightfield[y + 1][x + 1]);

            if (height < minZ) minZ = height;
            if (height > maxZ) maxZ = height;

            // Position
            v.pos.x = x * quadWidth;
            v.pos.y = y * quadWidth;
            v.pos.z = height;

            // Normal calc
           // f32 fl = uncompressHeight(paddedHeightfield[y][x]);     // front left
            f32 f = uncompressHeight(paddedHeightfield[y][x + 1]); // front
            //f32 fr = uncompressHeight(paddedHeightfield[y][x + 2]); // front right

            f32 l = uncompressHeight(paddedHeightfield[y + 1][x]); // left
            f32 r = uncompressHeight(paddedHeightfield[y + 1][x + 2]); // right

            //f32 bl = uncompressHeight(paddedHeightfield[y + 2][x]); // back left
            f32 b = uncompressHeight(paddedHeightfield[y + 2][x + 1]); // back
            //f32 br = uncompressHeight(paddedHeightfield[y + 2][x + 2]); // back right

            //https://gamedev.stackexchange.com/questions/165575/calculating-normal-map-from-height-map-using-sobel-operator
            // Sobel filter
          /*  const f32 dX = (fr + 2.0f * r + br) - (fl + 2.0f * l + bl);
            const f32 dY = (bl + 2.0f * b + br) - (fl + 2.0f * f + fr);
            const f32 dZ = quadWidth;*/
            f32v3 n(l - r, f - b, 2.0f);

            //f32v3 n(dX, dY, dZ);
            v.normalPacked = Pack_INT_2_10_10_10_REV(glm::normalize(n));
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

void TerrainMeshBuilder::setVertsWaterFromPaddedHeightfield(i32v2 cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {

    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;

    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = mWaterVerts[y * TERRAIN_MESH_WIDTH_VERTS + x];
            v.pos.x = cornerPosRelativeToRoot.x + x * quadWidth;
            v.pos.y = cornerPosRelativeToRoot.y + y * quadWidth;
            v.pos.z = 0.0f;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.depth = glm::max(-height, 0.0f);
        }
    }
}