#pragma once

#include "rendering/mesh/Vertex.h"
#include "world/TerrainConstants.h"
#include "terrain/CompressedHeight.h"

class TerrainMesh;

class TerrainMeshBuilder
{
public:

    static void initStaticIBO();

    void buildFromPaddedHeightfield(const f32v2& worldPosTreeRoot, const f32v2& cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {
        setVertsTerrainFromPaddedHeightfield(worldPosTreeRoot, cornerPosRelativeToRoot, totalWidth, paddedHeightfield);
        setVertsWaterFromPaddedHeightfield(cornerPosRelativeToRoot, totalWidth, paddedHeightfield);
    }

    void finishMeshes(TerrainMesh& terrainMesh, TerrainMesh& waterMesh, const f32v3& worldPosTreeRoot);
private:
    void setVertsTerrainFromPaddedHeightfield(const f32v2& worldPosTreeRoot, const f32v2& cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void setVertsWaterFromPaddedHeightfield(const f32v2& cornerPosRelativeToRoot, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    TerrainVertex mTerrainVerts[TERRAIN_MESH_SIZE_VERTS];
    WaterVertex mWaterVerts[WATER_MESH_SIZE_VERTS];
    BoundingSphere mBoundingSphere;
    f32v2 mUVRoot = f32v2(0.0f);
    f32v2 mBiomeUVRoot = f32v2(0.0f);

    static VGBuffer sTerrainIboUI32;
};

