#pragma once

#include "tile/Tile.h"

class MeshBuilder;
class Chunk;

struct HeightmapPatchData;
struct TileHandle;

namespace TileMeshBuilderMethods {
    void addBlock(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData);
    void addBlockVertical(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData);
    void addFloor(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData);
    void addFloorTerrainAligned(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData);
};

