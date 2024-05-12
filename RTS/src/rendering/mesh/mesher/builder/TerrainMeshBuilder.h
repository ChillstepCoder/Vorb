#pragma once

#include "rendering/mesh/Vertex.h"
#include "world/TerrainConstants.h"
#include "terrain/CompressedHeight.h"
#include "world/road/TerrainTextureType.h"

class TerrainMesh;
class RoadGrid;

class TerrainMeshBuilder
{
public:

    static void initStaticIBO();

    void buildFromPaddedHeightfield(
        i32v2 worldPosTreeRoot,
        i32v2 cornerPosRelativeToRoot,
        f32 totalWidth,
        const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS],
        const RoadGrid& roadGrid
    ) {
        setVertsTerrainFromPaddedHeightfield(worldPosTreeRoot, cornerPosRelativeToRoot, totalWidth, paddedHeightfield, roadGrid);
        setVertsWaterFromPaddedHeightfield(cornerPosRelativeToRoot, totalWidth, paddedHeightfield);
    }

    void finishMeshes(TerrainMesh& terrainMesh, TerrainMesh& waterMesh, const f32v3& worldPosTreeRoot);

public:
    TerrainTextureType mBaseTerrainLayers[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
private:
    void setVertsTerrainFromPaddedHeightfield(
        i32v2 worldPosTreeRoot,
        i32v2 cornerPosRelativeToRoot,
        f32 totalWidth,
        const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS],
        const RoadGrid& roadGrid);
    void setVertsWaterFromPaddedHeightfield(i32v2 cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    TerrainVertex mTerrainVerts[TERRAIN_MESH_SIZE_VERTS];
    WaterVertex mWaterVerts[WATER_MESH_SIZE_VERTS];
    BoundingSphere mBoundingSphere;
    f32v2 mUVRoot = f32v2(0.0f);
    f32v2 mBiomeUVRoot = f32v2(0.0f);
    i32v2 mWorldPosPatchCorner = i32v2(0);

    static VGBuffer sTerrainIboUI32;
};

