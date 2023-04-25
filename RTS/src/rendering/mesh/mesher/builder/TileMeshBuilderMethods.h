#pragma once

#include "tile/Tile.h"

class ProceduralMeshBuilder;

struct HeightmapPatchData;
struct TileHandle;
struct MaterialData;
class ContainerMeshBuilders;
class StaticPhysicsMeshBuilder;

namespace TileMeshBuilderMethods {
    void meshTileContainer(ContainerMeshBuilders& builders, StaticPhysicsMeshBuilder& physics, OPT const f32* heightData);

    void addBlock(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockVertical(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockWorldTiling(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addFloor(ProceduralMeshBuilder& meshBuilder, TileShape adjacentShapes[4], f32 floorHeight, const ui32v3& tileXYZ, const MaterialData& materialData, StaticPhysicsMeshBuilder& physMesh);
    void addCeiling(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, const MaterialData& materialData, StaticPhysicsMeshBuilder& physMesh);
    void addFloorTerrainAligned(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
    void addStairs(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, float tileGroundZOffset, Cartesian tileOrientation, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    f32 getModelRotationAtPosition(const f32v3& worldPos);

    // Gets wooblyness of buildings based on XYZ tile offset
    f32v2 getStructureWoobleAtPoint(const ui32v3& xyz);
    f32v2 getStructureWoobleAtPoint(ui32 x, ui32 y, ui32 z);
};
