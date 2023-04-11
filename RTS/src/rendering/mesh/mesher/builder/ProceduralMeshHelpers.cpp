#include "stdafx.h"
#include "ProceduralMeshHelpers.h"

#include "tile/TileWallContainer.h"
#include "tile/TileSpatialGrid.h"
#include "tile/Tile.h"

#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "physics/StaticPhysicsMeshBuilder.h"

struct WallVertexPermutation {
    f32 outerXOffsets[4]; // SW,SE,NW,NE -1, 0 or 1. Multiply by WALL_HALF_THICKNESS
    bool hasWestCap;
    bool hasEastCap;
    ui8 endCapCount;
};
static WallVertexPermutation sWallVertexPermutationLookup[64]; // 2^6 combinations for 6 bits 0-5

// TODO: This could be parameter maybe?
constexpr f32 WALL_THICKNESS = 0.15f;
constexpr f32 WALL_HALF_THICKNESS = WALL_THICKNESS / 2.0f;

RUNTIME_INIT_FUNC(SetupOuterWallVertexOffsetsTable) {
    // Compute corner vertex offsets assuming Cartesian::South, if Cartesian::West we will simply rotate them later
    // according to rotatedOffset = (offset.y, -offset.x)
    // X = origin = (0,0)
    // 
    //    4   5
    //    |   |
    // 2--X-c- --3
    //    |   |
    //    0   1
    // 
    // All wall/window/door shapes use these outer vertex offsets
    // The default values are if there are no adjacent walls to pull/push our vertices

    for (ui32 i = 0; i < 64; ++i) {
        bool bits[6];
        bits[0] = i & 0b00000001;
        bits[1] = i & 0b00000010;
        bits[2] = i & 0b00000100;
        bits[3] = i & 0b00001000;
        bits[4] = i & 0b00010000;
        bits[5] = i & 0b00100000;
        WallVertexPermutation& permutation = sWallVertexPermutationLookup[i];
        // Offsets should be multiplied by the half wall thickness
        permutation.outerXOffsets[0] = 0.0f; //SW
        permutation.outerXOffsets[1] = 0.0f; //SE
        permutation.outerXOffsets[2] = 0.0f; //NW
        permutation.outerXOffsets[3] = 0.0f; //NE
        permutation.hasWestCap = true;
        permutation.hasEastCap = true;

        // TODO: its possible to treat the 3 walls as a 3 bit binary, and use a switch
        // to determine the results, which MAY be more efficient than these if/else chains. Profile it!
        // TODO: Also is there a way to boil this ENTIRE table down to a LUT with 6 bits for 0-5? YES!
        { // SW
            constexpr int SW = 0;
            if (bits[0]) {
                permutation.hasWestCap = false;
                permutation.outerXOffsets[SW] = 1.0f;
            }
            else if (bits[2]) {
                permutation.hasWestCap = false;
            }
            else if (bits[4]) {
                permutation.hasWestCap = false;
                permutation.outerXOffsets[SW] = -1.0f;
            }
        }
        { // SE
            constexpr int SE = 1;
            if (bits[1]) {
                permutation.hasEastCap = false;
                permutation.outerXOffsets[SE] = -1.0f;
            }
            else if (bits[3]) {
                permutation.hasEastCap = false;
            }
            else if (bits[5]) {
                permutation.hasEastCap = false;
                permutation.outerXOffsets[SE] = 1.0f;
            }
        }
        { // NW
            constexpr int NW = 2;
            if (bits[4]) {
                permutation.hasWestCap = false;
                permutation.outerXOffsets[NW] = 1.0f;
            }
            else if (bits[2]) {
                permutation.hasWestCap = false;
            }
            else if (bits[0]) {
                permutation.hasWestCap = false;
                permutation.outerXOffsets[NW] = -1.0f;
            }
        }
        { // NE
            constexpr int NE = 3;
            if (bits[5]) {
                permutation.hasEastCap = false;
                permutation.outerXOffsets[NE] = -1.0f;
            }
            else if (bits[3]) {
                permutation.hasEastCap = false;
            }
            else if (bits[1]) {
                permutation.hasEastCap = false;
                permutation.outerXOffsets[NE] = 1.0f;
            }
        }
        permutation.endCapCount = (ui8)permutation.hasEastCap + (ui8)permutation.hasWestCap;
    }
}

void ProceduralMeshHelpers::addTileWallMesh(
    const TileData& tileData,
    const TileSpatialGrid& spatialGrid,
    const TileWallContainer& tileWalls,
    TileIndex index,
    Cartesian dir,
    const i32v3& tilePos,
    f32 wallHeight,
    ProceduralMeshBuilder& meshBuilder,
    StaticPhysicsMeshBuilder& physMesh
) {

    const i32v3& dims = spatialGrid.getDims();

    // Construct 6 bit binary adjacency code so we can look up the permutation in the cached table
    ui32 adjacencyCode = 0;
    if (dir == Cartesian::SOUTH) {
        // South wall example - numbered represent adjacent wall possibilities
        //    4   5
        //    |   |
        // 2-- -c- --3
        //    |   |
        //    0   1
        
        // 4
        adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(index).isValid() << 4;
        if (!spatialGrid.isPosAtSouthBorder(tilePos)) {
            // 0
            adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(spatialGrid.getSouthTileIndex(index)).isValid();
            if (!spatialGrid.isPosAtEastBorder(tilePos)) {
                // 1
                adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(spatialGrid.getSouthEastTileIndex(index)).isValid() << 1;
            }
        }
        // 2
        if (!spatialGrid.isPosAtWestBorder(tilePos)) {
            adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(spatialGrid.getWestTileIndex(index)).isValid() << 2;
        }
        if (!spatialGrid.isPosAtEastBorder(tilePos)) {
            const TileIndex eastIndex = spatialGrid.getEastTileIndex(index);
            // 3
            adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(eastIndex).isValid() << 3;
            // 5
            adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(eastIndex).isValid() << 5;
        }
    }
    else if (dir == Cartesian::WEST) {
        // West wall example (rotated)
        //     2
        //     |
        //  0-- --4
        //     |c
        //  1-- --5
        //     |
        //     3

        // 5
        adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(index).isValid() << 5;
        if (!spatialGrid.isPosAtSouthBorder(tilePos)) {
            // 3
            adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(spatialGrid.getSouthTileIndex(index)).isValid() << 3;
        }
        if (!spatialGrid.isPosAtWestBorder(tilePos)) {
            // 1
            adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(spatialGrid.getWestTileIndex(index)).isValid() << 1;
            if (!spatialGrid.isPosAtNorthBorder(tilePos)) {
                // 0
                adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(spatialGrid.getNorthWestTileIndex(index)).isValid();
            }
        }
        if (!spatialGrid.isPosAtNorthBorder(tilePos)) {
            const TileIndex northIndex = spatialGrid.getNorthTileIndex(index);
            // 4
            adjacencyCode |= (ui32)tileWalls.getSouthWallAtTile(northIndex).isValid() << 4;
            // 2
            adjacencyCode |= (ui32)tileWalls.getWestWallAtTile(northIndex).isValid() << 2;
        }
    }
    else {
        assert(false && "Unsupported cartesian");
    }

    assert(adjacencyCode < 64);

    const WallVertexPermutation& permutation = sWallVertexPermutationLookup[adjacencyCode];

    // Negative face vertices
    const f32v3 tilePosF = tilePos;
    const f32 scaledXOffsets[4] = {
        permutation.outerXOffsets[0] * WALL_HALF_THICKNESS,
        permutation.outerXOffsets[1] * WALL_HALF_THICKNESS,
        permutation.outerXOffsets[2] * WALL_HALF_THICKNESS,
        permutation.outerXOffsets[3] * WALL_HALF_THICKNESS
    };

    // Stored as SOUTH, NORTH, ENDCAP_WEST, ENDCAP_EAST
    constexpr CubeFacing CUBE_FACINGS[2][4] = {
        { CubeFacing::FRONT, CubeFacing::BACK, CubeFacing::LEFT, CubeFacing::RIGHT }, // SOUTH
        { CubeFacing::LEFT, CubeFacing::RIGHT, CubeFacing::BACK, CubeFacing::FRONT }, // WEST
    };

    // Helper for UV position
    auto computeTilingUVX = [](f32v4& outUVs, f32 xCoord, f32 quadWidth) {
        constexpr i32 MATERIAL_WIDTH_TILES = 4;
        f32 integer;
        f32 fractional = std::modff(xCoord, &integer);
        integer = (int)integer % MATERIAL_WIDTH_TILES;
        outUVs.x = (integer + fractional) / (f32)MATERIAL_WIDTH_TILES;
        outUVs.z = quadWidth / (f32)MATERIAL_WIDTH_TILES;
    };
    auto computeTilingUVXZ = [wallHeight](f32v4& outUVs, f32 xCoord, f32 zCoord, f32 quadWidth, f32 quadHeight) {
        constexpr i32 MATERIAL_WIDTH_TILES = 4;
        // X
        f32 integer;
        f32 fractional = std::modff(xCoord, &integer);
        integer = (int)integer % MATERIAL_WIDTH_TILES;
        outUVs.x = (integer + fractional) / (f32)MATERIAL_WIDTH_TILES;
        outUVs.z = quadWidth / (f32)MATERIAL_WIDTH_TILES;
        // Z
        integer;
        fractional = std::modff(zCoord, &integer);
        integer = (int)integer % (int)wallHeight;
        outUVs.y = (integer + fractional) / wallHeight;
        outUVs.w = quadHeight / wallHeight;
    };

    const f32v2 southFaceDims(1.0f - scaledXOffsets[0] + scaledXOffsets[1], wallHeight);
    const f32v2 northFaceDims(1.0f - scaledXOffsets[2] + scaledXOffsets[3], wallHeight);

    f32v2 southFaceOffset;
    f32v2 northFaceOffset;
    f32 uvFlip = 1.0f;
    if (dir == Cartesian::SOUTH) {
        southFaceOffset = f32v2(scaledXOffsets[0], -WALL_HALF_THICKNESS);
        northFaceOffset = f32v2(scaledXOffsets[2], WALL_HALF_THICKNESS);
    }
    else {
        // West rotation
        southFaceOffset = f32v2(-WALL_HALF_THICKNESS, -scaledXOffsets[1]);
        northFaceOffset = f32v2(WALL_HALF_THICKNESS, -scaledXOffsets[3]);
    }

    const f32v3 southRootPos(tilePosF.x + southFaceOffset.x, tilePosF.y + southFaceOffset.y, tilePosF.z * wallHeight);
    const f32v3 northRootPos(tilePosF.x + northFaceOffset.x, tilePosF.y + northFaceOffset.y, tilePosF.z * wallHeight);

    const f32v2 boardUVScale(1.0f);

    // Build specific mesh data
    switch (tileData.shape) {
        case TileShape::WALL: {
            f32v4 uvRectNorth(0.0f, 0.0f, 0.0f, 1.0f);
            f32v4 uvRectSouth(0.0f, 0.0f, 0.0f, 1.0f);
            computeTilingUVX(uvRectSouth, tilePosF[e_cast(dir)] + southFaceOffset[e_cast(dir)], southFaceDims.x);
            computeTilingUVX(uvRectNorth, tilePosF[e_cast(dir)] + northFaceOffset[e_cast(dir)], northFaceDims.x);
            // Mesh
            meshBuilder.addAxisAlignedQuad(southRootPos, southFaceDims, CUBE_FACINGS[e_cast(dir)][0], tileData.materialData[0], uvRectSouth, COLOR_WHITE);
            meshBuilder.addAxisAlignedQuad(northRootPos, northFaceDims, CUBE_FACINGS[e_cast(dir)][1], tileData.materialData[0], uvRectNorth, COLOR_WHITE);
            // Physics
            physMesh.addTileQuad(southRootPos, southFaceDims, CUBE_FACINGS[e_cast(dir)][0]);
            physMesh.addTileQuad(northRootPos, northFaceDims, CUBE_FACINGS[e_cast(dir)][1]);
            break;
        }
        case TileShape::WINDOW: {
            f32v4 uvRectSouth;
            f32v4 uvRectNorth;
            constexpr f32 WINDOW_HEIGHT = 1.2f;
            constexpr f32 WINDOW_BASE_Z = 0.9f;
            const f32 bottomQuadHeight = WINDOW_BASE_Z;
            const f32 topQuadHeight = wallHeight - WINDOW_HEIGHT - bottomQuadHeight;
            // Bottom Quads
            const f32v2 southBottomDims(southFaceDims.x, bottomQuadHeight);
            computeTilingUVXZ(uvRectSouth, tilePosF[e_cast(dir)] + southFaceOffset[e_cast(dir)], southRootPos.z, southBottomDims.x, southBottomDims.y);
            meshBuilder.addAxisAlignedQuad(southRootPos, southBottomDims, CUBE_FACINGS[e_cast(dir)][0], tileData.materialData[0], uvRectSouth, COLOR_WHITE);
            const f32v2 northBottomDims(northFaceDims.x, bottomQuadHeight);
            computeTilingUVXZ(uvRectNorth, tilePosF[e_cast(dir)] + northFaceOffset[e_cast(dir)], northRootPos.z, northBottomDims.x, northBottomDims.y);
            meshBuilder.addAxisAlignedQuad(northRootPos, northBottomDims, CUBE_FACINGS[e_cast(dir)][1], tileData.materialData[0], uvRectNorth, COLOR_WHITE);
            // Top Quads
            const f32v2 southTopDims(southFaceDims.x, topQuadHeight);
            const f32v3 southTopRoot(southRootPos.x, southRootPos.y, southRootPos.z + bottomQuadHeight + WINDOW_HEIGHT);
            computeTilingUVXZ(uvRectSouth, tilePosF[e_cast(dir)] + southFaceOffset[e_cast(dir)], southTopRoot.z, southTopDims.x, southTopDims.y);
            meshBuilder.addAxisAlignedQuad(southTopRoot, southTopDims, CUBE_FACINGS[e_cast(dir)][0], tileData.materialData[0], uvRectSouth, COLOR_WHITE);
            const f32v2 northTopDims(northFaceDims.x, topQuadHeight);
            const f32v3 northTopRoot(northRootPos.x, northRootPos.y, northRootPos.z + bottomQuadHeight + WINDOW_HEIGHT);
            computeTilingUVXZ(uvRectNorth, tilePosF[e_cast(dir)] + northFaceOffset[e_cast(dir)], northTopRoot.z, northTopDims.x, northTopDims.y);
            meshBuilder.addAxisAlignedQuad(northTopRoot, northTopDims, CUBE_FACINGS[e_cast(dir)][1], tileData.materialData[0], uvRectNorth, COLOR_WHITE);

            // Rim boards
            // p3 p4
            // p1 p2
            f32v2 boardHalfDims(WALL_HALF_THICKNESS, WALL_HALF_THICKNESS + 0.05f);
            f32v3 p1, p2, p3, p4;
            f32v3 normalDir;
            bool hasAdjacentWindow = false; // Used for removing a shared board
            p1 = f32v3(tilePosF.x, tilePosF.y, tilePosF.z * wallHeight + bottomQuadHeight);
            if (dir == Cartesian::SOUTH) {
                p2 = f32v3(p1.x + 1.0f, p1.y, p1.z);
                p3 = f32v3(p1.x, p1.y, p1.z + WINDOW_HEIGHT);
                p4 = f32v3(p3.x + 1.0f, p3.y, p3.z);
                normalDir = f32v3(0.0f, 1.0f, 0.0f);
                if (!spatialGrid.isPosAtEastBorder(tilePos)) {
                    hasAdjacentWindow = tileWalls.getSouthWallAtTile(spatialGrid.getEastTileIndex(index)).wallID == tileData.id;
                }
            }
            else {
                p2 = f32v3(p1.x, p1.y + 1.0f, p1.z);
                p3 = f32v3(p1.x, p1.y, p1.z + WINDOW_HEIGHT);
                p4 = f32v3(p3.x, p3.y + 1.0f, p3.z);
                normalDir = f32v3(1.0f, 0.0f, 0.0f);
                if (!spatialGrid.isPosAtNorthBorder(tilePos)) {
                    hasAdjacentWindow = tileWalls.getWestWallAtTile(spatialGrid.getNorthTileIndex(index)).wallID == tileData.id;
                }
            }
            // Horizontal
            meshBuilder.addBoardBetweenPoints(p1, p2, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            meshBuilder.addBoardBetweenPoints(p3, p4, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            // Vertical
            boardHalfDims.y -= 0.01f; // Prevent Z fighting
            meshBuilder.addBoardBetweenPoints(p1, p3, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            if (!hasAdjacentWindow) {
                meshBuilder.addBoardBetweenPoints(p2, p4, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            }

            // Physics
            physMesh.addTileQuad(southRootPos, southFaceDims, CUBE_FACINGS[e_cast(dir)][0]);
            physMesh.addTileQuad(northRootPos, northFaceDims, CUBE_FACINGS[e_cast(dir)][1]);
            break;
        }
        case TileShape::DOOR: {
            f32v4 uvRectSouth;
            f32v4 uvRectNorth;
            constexpr f32 DOOR_HEIGHT = 2.0f;
            // Top quad
            const f32v2 topDimsSouth(southFaceDims.x, wallHeight - DOOR_HEIGHT);
            const f32v3 topRootSouth(southRootPos.x, southRootPos.y, southRootPos.z + DOOR_HEIGHT);
            computeTilingUVXZ(uvRectSouth, tilePosF[e_cast(dir)] + southFaceOffset[e_cast(dir)], topRootSouth.z, topDimsSouth.x, topDimsSouth.y);
            meshBuilder.addAxisAlignedQuad(topRootSouth, topDimsSouth, CUBE_FACINGS[e_cast(dir)][0], tileData.materialData[0], uvRectSouth, COLOR_WHITE);
            const f32v2 topDimsNorth(northFaceDims.x, wallHeight - DOOR_HEIGHT);
            const f32v3 topRootNorth(northRootPos.x, northRootPos.y, northRootPos.z + DOOR_HEIGHT);
            computeTilingUVXZ(uvRectNorth, tilePosF[e_cast(dir)] + northFaceOffset[e_cast(dir)], topRootNorth.z, topDimsNorth.x, topDimsNorth.y);
            meshBuilder.addAxisAlignedQuad(topRootNorth, topDimsNorth, CUBE_FACINGS[e_cast(dir)][1], tileData.materialData[0], uvRectNorth, COLOR_WHITE);

            // Trim boards
            // p3 p4
            // p1 p2
            f32v2 boardHalfDims(WALL_HALF_THICKNESS - 0.025f, WALL_HALF_THICKNESS + 0.05f);
            f32v3 p1, p2, p3, p4;
            f32v3 normalDir;
            p1 = f32v3(tilePosF.x, tilePosF.y, tilePosF.z * wallHeight);
            if (dir == Cartesian::SOUTH) {
                p2 = f32v3(p1.x + 1.0f, p1.y, p1.z);
                p3 = f32v3(p1.x, p1.y, p1.z + DOOR_HEIGHT);
                p4 = f32v3(p3.x + 1.0f, p3.y, p3.z);
                normalDir = f32v3(0.0f, 1.0f, 0.0f);
            }
            else {
                p2 = f32v3(p1.x, p1.y + 1.0f, p1.z);
                p3 = f32v3(p1.x, p1.y, p1.z + DOOR_HEIGHT);
                p4 = f32v3(p3.x, p3.y + 1.0f, p3.z);
                normalDir = f32v3(1.0f, 0.0f, 0.0f);
            }
            meshBuilder.addBoardBetweenPoints(p3, p4, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            boardHalfDims.y -= 0.01f; // Prevent Z fighting
            meshBuilder.addBoardBetweenPoints(p1, p3, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);
            meshBuilder.addBoardBetweenPoints(p2, p4, boardHalfDims, tileData.materialData[1], boardUVScale, normalDir);

            // TODO: Replace with dynamic mesh!
            // Door board
            const f32v2 doorDims(1.0f, DOOR_HEIGHT);
            meshBuilder.addAxisAlignedQuad(southRootPos, doorDims, CUBE_FACINGS[e_cast(dir)][0], tileData.materialData[2], f32v4(0.0f, 0.0f, 1.0f, 1.0f), COLOR_WHITE);
            meshBuilder.addAxisAlignedQuad(northRootPos, doorDims, CUBE_FACINGS[e_cast(dir)][1], tileData.materialData[2], f32v4(0.0f, 0.0f, 1.0f, 1.0f), COLOR_WHITE);

            // Physics
            physMesh.addTileQuad(topRootSouth, topDimsSouth, CUBE_FACINGS[e_cast(dir)][0]);
            physMesh.addTileQuad(topRootNorth, topDimsNorth, CUBE_FACINGS[e_cast(dir)][1]);
            break;
        }
        default:
            assert(false);
            break;

    }
    // TODO: Endcaps

}
