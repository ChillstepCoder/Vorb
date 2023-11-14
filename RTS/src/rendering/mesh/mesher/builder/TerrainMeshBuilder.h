#pragma once

#include "rendering/mesh/Vertex.h"
#include "world/TerrainConstants.h"
#include "terrain/CompressedHeight.h"

class Mesh;

class TerrainMeshBuilder
{
public:

    static void initStaticIBO();

    void buildFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {
        setVertsTerrainFromPaddedHeightfield(cornerPos, totalWidth, paddedHeightfield);
        setVertsWaterFromPaddedHeightfield(cornerPos, totalWidth, paddedHeightfield);
    }


    void finishMeshes(Mesh& terrainMesh, Mesh& waterMesh, const f32v3& worldPos);
private:
    void setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    TerrainVertex mTerrainVerts[TERRAIN_MESH_SIZE_VERTS];
    WaterVertex mWaterVerts[WATER_MESH_SIZE_VERTS];
    BoundingSphere mBoundingSphere;

    static VGBuffer sTerrainIboUI32;
};

