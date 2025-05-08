#pragma once

#include "rendering/mesh/Vertex.h"
#include "world/TerrainConstants.h"
#include "terrain/CompressedHeight.h"
#include "world/road/TerrainSurfaceType.h"

class TerrainMesh;
class TerrainSurfaceGrid;

struct TerrainSurfaceData {
    TerrainSurfaceType baseTexture = TerrainSurfaceType::None;
    ui8 baseDensityTextureID = 0;
    TerrainSurfaceOverlayType overlayTexture = TerrainSurfaceOverlayType::None;
    ui8 overlayDensityTextureID = 0;
};
static_assert(sizeof(TerrainSurfaceData) == 4, "Currently packing into RGBA8");

class TerrainMeshBuilder
{
public:

    static void initStaticIBO();

    void buildFromPaddedHeightfield(
        i32v2 worldPosTreeRoot,
        i32v2 cornerPosRelativeToRoot,
        f32 totalWidth,
        const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS],
        const TerrainSurfaceGrid& roadGrid
    ) {
        setVertsTerrainFromPaddedHeightfield(worldPosTreeRoot, cornerPosRelativeToRoot, totalWidth, paddedHeightfield, roadGrid);
        setVertsWaterFromPaddedHeightfield(cornerPosRelativeToRoot, totalWidth, paddedHeightfield);
    }

    void finishMeshes(TerrainMesh& terrainMesh, TerrainMesh& waterMesh, const f32v3& worldPosTreeRoot);

public:
    TerrainSurfaceData mTerrainSurfaceLayers[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
private:
    void setVertsTerrainFromPaddedHeightfield(
        i32v2 worldPosTreeRoot,
        i32v2 cornerPosRelativeToRoot,
        f32 totalWidth,
        const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS],
        const TerrainSurfaceGrid& roadGrid);
    void setVertsWaterFromPaddedHeightfield(i32v2 cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    TerrainVertex mTerrainVerts[TERRAIN_MESH_SIZE_VERTS];
    WaterVertex mWaterVerts[WATER_MESH_SIZE_VERTS];
    BoundingSphere mBoundingSphere;
    f32v2 mUVRoot = f32v2(0.0f);
    f32v2 mBiomeUVRoot = f32v2(0.0f);
    i32v2 mWorldPosPatchCorner = i32v2(0);

    static VGBuffer sTerrainIboUI32;
};

