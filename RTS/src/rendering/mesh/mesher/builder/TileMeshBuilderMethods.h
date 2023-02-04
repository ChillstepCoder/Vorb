#pragma once

#include "tile/Tile.h"

class BillboardMeshBuilder;
class ProceduralMeshBuilder;
class Chunk;

struct HeightmapPatchData;
struct TileHandle;
class ContainerMeshBuilders;
class TileContainer;
class StaticPhysicsMeshBuilder;

namespace TileMeshBuilderMethods {
    void meshTileContainer(ContainerMeshBuilders& builders, StaticPhysicsMeshBuilder& physics, OPT const f32* heightData);

    void addBlock(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockVertical(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addBlockWorldTiling(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addFloor(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addFloorTerrainAligned(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
    void addStairs(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, StaticPhysicsMeshBuilder& physMesh);
    void addWall(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileData& tileData, Cartesian dir, f32 height, StaticPhysicsMeshBuilder& physMesh);
    f32 getModelRotationAtPosition(const f32v3& worldPos);
};
