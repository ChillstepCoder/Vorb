#include "stdafx.h"
#include "TileMeshBuilderMethods.h"

#include "rendering/mesh/MeshBuilder.h"

#include "world/TileHandle.h"
#include "world/Chunk.h"


constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 5;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_HEIGHT = 3;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_WIDTH = 1;

// TODO: Method(s) file?
struct ConnectedWallData {
    union {
        struct {
            ui8 a;
            ui8 b;
            ui8 c;
            ui8 d;
        };
        ui8 dataArray[4];
        ui32 data;
    };
};

ConnectedWallData sConnectedWallData[256];
const ui16 sExposedWallLookup[4] = { 0x13, 0x12, 0x14, 0x15 }; // These are texture indexes corresponding to the walls

enum ExposedNeighbor8Bits {
    EN8_BOTTOM_LEFT = 1 << 0,
    EN8_BOTTOM = 1 << 1,
    EN8_BOTTOM_RIGHT = 1 << 2,
    EN8_LEFT = 1 << 3,
    EN8_RIGHT = 1 << 4,
    EN8_TOP_LEFT = 1 << 5,
    EN8_TOP = 1 << 6,
    EN8_TOP_RIGHT = 1 << 7
};
enum ExposedNeighbor4Bits {
    EN4_BOTTOM = 1 << 0,
    EN4_LEFT = 1 << 1,
    EN4_RIGHT = 1 << 2,
    EN4_TOP = 1 << 3,
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 5.0f);
const f32v2 VERTICAL_WALL_DIMS = f32v2(1.0f, 3.0f);

inline bool isBitSet(int v, ExposedNeighbor8Bits bit) {
    return v & bit;
}

inline bool isBitSet(int v, ExposedNeighbor4Bits bit) {
    return v & bit;
}

inline bool areAnyBitsSet(int v, int bits) {
    return v & bits;
}

inline bool areBitsSet(int v, int bits) {
    return (v & bits) == bits;
}

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_CARDINAL[4] = {
    EN8_BOTTOM,
    EN8_LEFT,
    EN8_RIGHT,
    EN8_TOP
};

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_ADJACENTS[4][2] = {
    { EN8_LEFT, EN8_RIGHT }, // EN8_BOTTOM
    { EN8_BOTTOM, EN8_TOP }, // EN8_LEFT
    { EN8_BOTTOM, EN8_TOP }, // EN8_RIGHT
    { EN8_LEFT, EN8_RIGHT }, // EN8_TOP
};
const ExposedNeighbor4Bits EXPOSED_NEIGHBOR_4_ADJACENTS[4][2] = {
    { EN4_LEFT, EN4_RIGHT }, // EN4_BOTTOM
    { EN4_BOTTOM, EN4_TOP }, // EN4_LEFT
    { EN4_BOTTOM, EN4_TOP }, // EN4_RIGHT
    { EN4_LEFT, EN4_RIGHT }, // EN4_TOP
};

const NeighborIndex8 EXPOSED_NEIGHBOR_8_ADJACENT_INDICES[4][2] = {
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_BOTTOM
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_LEFT
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_RIGHT
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_TOP
};

const NeighborIndex4 EXPOSED_NEIGHBOR_4_ADJACENT_INDICES[4][2] = {
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_BOTTOM
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_LEFT
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_RIGHT
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_TOP
};

const CubeFacing EXPOSED_NEIGHBOR_QUAD_FACINGS[4] = {
    CubeFacing::FRONT,
    CubeFacing::LEFT,
    CubeFacing::RIGHT,
    CubeFacing::BACK
};

f32v2 getUvsOffsetsFromConnectedWallIndex(int index) {
    f32v2 rv;
    rv.x = float(index % (int)CONNECTED_WALL_DIMS.x);
    rv.y = float(index / (int)CONNECTED_WALL_DIMS.x);
    return rv;
}

f32v2 getUvsOffsetsFromVerticalWallIndex(int index) {
    f32v2 rv;
    rv.x = 0.0f;
    rv.y = (2 - index) / 3.0f;
    return rv;
}

void TileMeshBuilderMethods::addBlock(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData) {
    const SubTexture& texture = tileData.texture;
    switch (tileData.textureMethod) {
        case TileTextureMethod::SIMPLE: {
            meshBuilder.addAxisAlignedQuad(
                f32v3(tileXY.x, tileXY.y, floorBaseHeight) + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
                f32v2(1.0f) /*dims*/,
                CubeFacing::TOP,
                texture,
                texture.mUvRect,
                COLOR_WHITE
            );
            break;
        }
        case TileTextureMethod::CONNECTED_WALL: {
            // todo: FIX (check history for old code)
            break;
        }
        case TileTextureMethod::VERTICAL: {
            addBlockVertical(meshBuilder, floorBaseHeight, tileXY, tileHandle, tileData);
            break;
        }
        case TileTextureMethod::WORLD_TILING: {
            // todo: FIX
            int xOff = (ui32)tileXY.x % 8;
            int yOff = 7 - (ui32)tileXY.y % 8;
            meshBuilder.addAxisAlignedQuad(
                f32v3(tileXY.x, tileXY.y, floorBaseHeight) + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
                f32v2(1.0f) /*dims*/,
                CubeFacing::TOP,
                texture,
                texture.mUvRect,
                COLOR_WHITE
            );
            break;
        }
        case TileTextureMethod::FLORA: {
            // We do not mesh flora in this pass
            break;
        }
    }
    static_assert((int)TileTextureMethod::COUNT == 6, "Implement geo generation for new method");
}

void TileMeshBuilderMethods::addBlockVertical(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData) {
    // Currently only supported for ground layer
    assert(tileData.layer == TILE_LAYER_GROUND);

    const SubTexture& texture = tileData.texture;
    const Tile& tile = *tileHandle.tile;

    const f32 baseZPosition = tile.getBaseZPositionUncompressedThreadSafe();
    const f32v3 tilePos(tileXY.x, tileXY.y, baseZPosition);

    /*TileHandle neighbors[4];
    chunk.getTileNeighbors4(tileHandle.index, neighbors);*/

    // Get height offsets to adjacent tiles
    //const f32 zPosition = tile.baseZPosition; // Dont check terrain here, assume above // TODO: make sure this is right
    const f32 heightDiff = baseZPosition - floorBaseHeight;
    f32 heightDiffs[4];
    // f32 occluderHeight
    // TODO: Fix this
    for (int i = 0; i < 4; ++i) {
        heightDiffs[i] = heightDiff;
    }
    // Old culling
    /*heightDiffs[(int)NeighborIndex4::BOTTOM] = baseZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::BOTTOM]);
    heightDiffs[(int)NeighborIndex4::LEFT] = baseZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::LEFT]);
    heightDiffs[(int)NeighborIndex4::RIGHT] = baseZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::RIGHT]);
    heightDiffs[(int)NeighborIndex4::TOP] = baseZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::TOP]);*/

    // Render top
    meshBuilder.addAxisAlignedQuad(
        tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
        f32v2(1.0f), // Dimensions
        CubeFacing::TOP,
        texture,
        f32v4(0.0f, 2.0f / 3.0f, 1.0f, 1.0f / 3.0f),
        COLOR_WHITE
    );

    // Render sides
    for (int c = 0; c < 4; ++c) {
        // Render exposed cardinal wall if needed
        if (heightDiffs[c] > 0.0f) {
            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            const f32v3 quadPos = tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
            const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(2);
            f32v4 uvs = texture.mUvRect;
            uvs.y += offsets.y * uvs.w;
            uvs.w /= 3.0f;

            meshBuilder.addAxisAlignedQuad(
                quadPos,
                f32v2(1.0f), // Dimensions
                quadFacing,
                texture,
                uvs,
                COLOR_WHITE
            );

            // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
            for (int i = 1; i < heightDiffs[c]; ++i) {
                unsigned sideCheck = 0;
                // New exposure check for left and right on towers
                ui16 val = 1;
                // Only bottom uses bottom texture
                if (i + 1.5f >= heightDiffs[c]) {
                    val = 2;
                }

                const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(val);
                f32v4 uvs = texture.mUvRect;
                // + 1 for the tall wall variants
                uvs.y += offsets.y * uvs.w;
                uvs.w /= 3.0f;
                // TODO: this resize here...

                // TODO: Stretched quads?
                meshBuilder.addAxisAlignedQuad(
                    f32v3(quadPos.x, quadPos.y, quadPos.z - i),
                    f32v2(1.0f), // Dimensions
                    quadFacing,
                    texture,
                    uvs,
                    COLOR_WHITE
                );
            }
        }
    }
}

void TileMeshBuilderMethods::addFloor(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData) {
    const SubTexture& texture = tileData.texture;

    /*f32 corners[4];
    mWorldGrid.computeTileCorners(heightData->data, TilePosition(chunk.getChunkID(), tileIndex), corners);

    if (isOnTerrain) {
        quadMeshBuilder.addTerrainAlignedQuad(
            tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
            corners,
            texture,
            COLOR_WHITE,
            mWorldGrid.areTrianglesFlippedAtTile(tileIndex)
        );
    }
    else {
        assert(false);
    }*/
}

void TileMeshBuilderMethods::addFloorTerrainAligned(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatchData* heightData, const TileHandle& tileHandle, const TileData& tileData) {
    const SubTexture& texture = tileData.texture;

    //f32 corners[4];
    //mWorldGrid.computeTileCorners(heightData->data, TilePosition(chunk.getChunkID(), tileIndex), corners);

    //if (isOnTerrain) {
    //    quadMeshBuilder.addTerrainAlignedQuad(
    //        tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
    //        corners,
    //        texture,
    //        COLOR_WHITE,
    //        mWorldGrid.areTrianglesFlippedAtTile(tileIndex)
    //    );
    //}
    //else {
    //    assert(false);
    //}
}

//
//f32 ChunkMesher::getTileHeight(TileFloor floor, const Tile& neighbor, const f32* heightData, TilePosition tilePos) {
//    f32 height = 0.0f;
//    const TileID tileId = neighbor.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
//    if (tileId != TILE_ID_NONE) {
//        //const TileData& tileData = TileRepository::getTileData(tileId);
//        height = neighbor.getBaseZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles do not count
//      /*  if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = neighbor.getBaseZPositionUncompressedThreadSafe();
//        }*/
//    }
//    return glm::max(height, mWorldGrid.computeMinHeightAtTile(heightData, tilePos));
//}
//
//f32 ChunkMesher::getTileHeight(TileFloor floor, const TileHandle& neighbor) {
//    const Tile& tile = *neighbor.tile;
//    f32 height = 0.0f;
//    const TileID tileId = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
//    if (tileId != TILE_ID_NONE) {
//        /* const TileData& tileData = TileRepository::getTileData(tileId);
//         const SpriteData& spriteData = tileData.spriteData;*/
//        height = tile.getBaseZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles appear to be 1 tile lower
//        /*if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = tile.getBaseZPositionUncompressedThreadSafe();
//        }*/
//    }
//    return glm::max(height, mWorldGrid.computeMinHeightAtTile(TilePosition(neighbor.chunk->getChunkID(), neighbor.index)));
//}
