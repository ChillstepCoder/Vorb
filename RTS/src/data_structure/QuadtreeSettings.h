#pragma once

#include "world/WorldData.h"

constexpr int GRASS_QUADTREE_MAX_LOD = 5;

constexpr ui32 TERRAIN_QUADTREE_MAX_LOD = 5;
constexpr ui32 CHUNKS_PER_TERRAIN_QUADTREE = 16; // Must be power of two
constexpr ui32 WORLD_WIDTH_TERRAIN_QUADTREES = WorldData::WORLD_WIDTH_CHUNKS / CHUNKS_PER_TERRAIN_QUADTREE;
constexpr ui32 WORLD_SIZE_TERRAIN_QUADTREES = SQ(WORLD_WIDTH_TERRAIN_QUADTREES);
constexpr ui32 TERRAIN_QUADTREE_WIDTH = CHUNKS_PER_TERRAIN_QUADTREE * CHUNK_WIDTH;

struct QuadtreeSettings {
    f32 distance;
    f32 distanceSq;
    f32 fadeDistance;
    f32 lodDistanceOffset;
};