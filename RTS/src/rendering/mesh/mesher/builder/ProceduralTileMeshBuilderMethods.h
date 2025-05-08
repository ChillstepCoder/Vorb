#pragma once

#include "tile/Tile.h"
#include "terrain/CompressedHeight.h"

class ProceduralMeshBuilder;

class HeightmapPatch;
class Tile;
struct MaterialDesc;
class ContainerMeshBuilders;
class StaticPhysicsMeshBuilder;

namespace ProceduralTileMeshBuilderMethods {
    void meshTileContainer(ContainerMeshBuilders& builders, StaticPhysicsMeshBuilder& physics, OPT const CompressedHeight* heightData);

    void addBlock(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockVertical(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockWorldTiling(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addFloor(ProceduralMeshBuilder& meshBuilder, TileShape adjacentShapes[4], f32 floorHeight, const ui32v3& tileXYZ, const MaterialDesc& materialData, StaticPhysicsMeshBuilder& physMesh);
    void addCeiling(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, const MaterialDesc& materialData, StaticPhysicsMeshBuilder& physMesh);
    //void addFloorTerrainAligned(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatch* heightData, const TileHandle& tileHandle, const TileDef& tileData);
    void addStairs(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, float tileGroundZOffset, Cartesian tileOrientation, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh);

    // Gets wooblyness of buildings based on XYZ tile offset
    f32v2 getStructureWoobleAtPoint(const ui32v3& xyz);
    f32v2 getStructureWoobleAtPoint(ui32 x, ui32 y, ui32 z);
};
