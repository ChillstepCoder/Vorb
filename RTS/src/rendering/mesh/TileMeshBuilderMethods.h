#pragma once

#include "world/Tile.h"

class MeshBuilder;
class Chunk;

struct HeightmapPatchData;
struct TileHandle;

namespace TileMeshBuilderMethods {
    void addBlock(MeshBuilder& meshBuilder, TileFloor floor, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData);
    void addBlockVertical(MeshBuilder& meshBuilder, TileFloor floor, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData);
    void addFloor(MeshBuilder& meshBuilder, TileFloor floor, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
    void addFloorTerrainAligned(MeshBuilder& meshBuilder, TileFloor floor, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
};

