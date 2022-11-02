#pragma once

#include "tile/Tile.h"

class BillboardMeshBuilder;
class MeshBuilder;
class Chunk;

struct HeightmapPatchData;
struct TileHandle;
class StaticPhysicsMeshBuilder;
class TileContainer;

namespace TileMeshBuilderMethods {
    void meshTileContainerStatic(MeshBuilder& meshBuilder, BillboardMeshBuilder* billboardMeshBuilder, const TileContainer& tileContainer, OPT StaticPhysicsMeshBuilder* physMesh);
    void meshTileContainerDynamic(MeshBuilder& meshBuilder, const TileContainer& tileContainer);

    void addBlock(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addBlockVertical(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addBlockWorldTiling(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addFloor(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addFloorTerrainAligned(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
    void addStairs(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addWall(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileData& tileData, Cartesian dir, f32 height, OPT StaticPhysicsMeshBuilder* physMesh);
};
