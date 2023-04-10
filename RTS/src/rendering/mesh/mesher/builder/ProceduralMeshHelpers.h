#pragma once

class ProceduralMeshBuilder;
class StaticPhysicsMeshBuilder;
class TileWallContainer;
class TileSpatialGrid;
struct TileData;

namespace ProceduralMeshHelpers
{
    //void addWindowMesh(ui32v3 tileDims, f32 floorHeight, ProceduralMeshBuilder& meshBuilder, StaticPhysicsMeshBuilder& physMesh);
    void addTileWallMesh(
        const TileData& tileData,
        const TileSpatialGrid& spatialGrid,
        const TileWallContainer& tileWalls,
        TileIndex index,
        Cartesian dir,
        const i32v3& tilePos,
        f32 wallHeight,
        ProceduralMeshBuilder& meshBuilder,
        StaticPhysicsMeshBuilder& physMesh
    );
};

