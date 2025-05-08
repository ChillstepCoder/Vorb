#pragma once

class ProceduralMeshBuilder;
class StaticPhysicsMeshBuilder;
class TileWallContainer;
class TileSpatialGrid;
class Tile;
class TileDef;

// TODO: This can be improved, we can have a generic "procedural tile" where it is data driven
// as a model with special parameters
// Then we can edit this simple model in editor, to quickly generate beams + optimize
// Things such as roofs and support structure which are separate objects on the Building, can have
// their own in editor tweakers
// The "TileMeshEditor" will be the tool to bring everything together.
// Interior faces can be automatically removed? https://blender.stackexchange.com/questions/18916/how-to-remove-interior-faces-while-keeping-exterior-faces-untouched
namespace ProceduralMeshHelpers
{
    //void addWindowMesh(ui32v3 tileDims, f32 floorHeight, ProceduralMeshBuilder& meshBuilder, StaticPhysicsMeshBuilder& physMesh);
    void addTileWallMesh(
        const TileDef& tileData,
        const TileSpatialGrid& spatialGrid,
        const TileWallContainer& tileWalls,
        const std::vector<Tile>& tiles,
        TileIndex index,
        Cartesian dir,
        const i32v3& tilePos,
        f32 wallHeight,
        ProceduralMeshBuilder& meshBuilder,
        std::unordered_set<MaterialID>& materialDependencies,
        StaticPhysicsMeshBuilder& physMesh
    );
};

