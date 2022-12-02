#pragma once

#include "tile/Tile.h"

class BillboardMeshBuilder;
class ProceduralMeshBuilder;
class Chunk;

struct HeightmapPatchData;
struct TileHandle;
class StaticPhysicsMeshBuilder;
class TileContainer;
class InstancedStaticModelGatherer;

namespace TileMeshBuilderMethods {
    void meshTileContainerStatic(
        ProceduralMeshBuilder& meshBuilder,
        BillboardMeshBuilder* billboardMeshBuilder,
        InstancedStaticModelGatherer& modelGatherer,
        const TileContainer& tileContainer,
        OPT StaticPhysicsMeshBuilder* physMesh,
        OPT const f32* heightData = nullptr
    );
    void meshTileContainerDynamic(ProceduralMeshBuilder& meshBuilder, const TileContainer& tileContainer);

    void addBlock(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addBlockVertical(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addBlockWorldTiling(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addFloor(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addFloorTerrainAligned(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
    void addStairs(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMeshBuilder* physMesh);
    void addWall(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const TileData& tileData, Cartesian dir, f32 height, OPT StaticPhysicsMeshBuilder* physMesh);
};
