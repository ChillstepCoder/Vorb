#include "stdafx.h"
#include "TileMeshBuilderMethods.h"

#include "rendering/mesh/MeshBuilder.h"

#include "tile/TileHandle.h"
#include "tile/Stairs.h"
#include "world/Chunk.h"

#include "physics/StaticPhysicsMesh.h"
#include "resources/TileRepository.h"

constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 5;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_HEIGHT = 3;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_WIDTH = 1;

enum class WallCornerType {
    FLAT, // No change to edge length
    INWARD, // Shrink edg
    OUTWARD,
    NONE, // Render no flat edge
};

// Track what quads are at a tile
struct WallChainData {
    f32v3 startPos;
    f32 length;
    f32 groundPos;
    WallCornerType cornerTypeStart;
    WallCornerType cornerTypeEnd;
    Cartesian dir;
    TileID tileId;
    bool isPrimary;
};

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

struct PrevWallIndices {
    PrevWallIndices() : indices{ UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX } {};
    union {
        struct {
            ui32 indexInA;
            ui32 indexOutA;
            ui32 indexInB;
            ui32 indexOutB;
        };
        ui32 indices[4];
    };
};

constexpr f32 WALL_THICKNESS = 0.1f;
constexpr f32 OUTSIDE_EPSILON = 0.01f;
constexpr f32 WALL_THICKNESS_PLUS_EPSILON = WALL_THICKNESS + OUTSIDE_EPSILON;

void mergeOrMakeWallFace(const TileContainer& tileContainer, ui32& prevIndex, std::vector<WallChainData>& wallData, f32 groundZPosition, const TileWalls& walls, ui32 wallCartesian, Cartesian wallDir, ui32 x, ui32 y, ui32 z, f32 xAdd, f32 yAdd, bool isInside)
{
    bool didMerge = false;
    if (prevIndex != UINT32_MAX) {
        WallChainData& prevWall = wallData[prevIndex];
        // We can only merge if same height, same tile ID, and not a door
        if (prevWall.groundPos == groundZPosition && prevWall.tileId == walls.walls[wallCartesian].wallID) {
            // Extend previous wall
            ++prevWall.length;
            didMerge = true;
        }
    }
    if (!didMerge) {
        // Check if this is a door and if so ignore it because its a dynamic object not part of this mesh
        const TileID tileId = walls.walls[wallCartesian].wallID;
        if (TileRepository::getTileData(tileId).shape == TileShape::DOOR) {
            prevIndex = UINT32_MAX;
            return;
        }
        prevIndex = wallData.size();
        WallChainData& newWall = wallData.emplace_back();
        // Add new wall
        // TODO: No copy paste
        newWall.tileId = tileId;
        newWall.dir = wallDir;
        newWall.length = 1;
        newWall.groundPos = groundZPosition;
        newWall.startPos = f32v3(x + xAdd, y + yAdd, z * tileContainer.getFloorHeight());
        newWall.cornerTypeStart = WallCornerType::FLAT;
        newWall.isPrimary = isInside;
    }         
}

void mergeOrMakeSouthNorthWall(const TileContainer& tileContainer, ui32 x, ui32 y, ui32 z, PrevWallIndices& prevSouthNorthWallIndices, std::vector<WallChainData>& wallData, int isNorth) {
    const ui32v3& dims = tileContainer.getDims();
    TileIndex tileIndex = tileContainer.getTileIndexFromXYZOffset(x, y, z);
    const Tile& tile = tileContainer.getTileAt(tileIndex);
    const f32 groundZPosition = tile.getGroundZPositionUncompressedMainThread();
    // TODO: Greedy meshing
    const TileWalls& walls = tileContainer.getWallsMainThread(tileIndex);
    // TODO: Use paint ID
    ui32& prevInnerIndex = prevSouthNorthWallIndices.indices[isNorth * 2];
    ui32& prevOuterIndex = prevSouthNorthWallIndices.indices[isNorth * 2 + 1];
    const ui32 wallCartesian = isNorth * 3; // Match cartesian
    if (walls.walls[wallCartesian].wallID != TILE_ID_NONE) {
        // We have a wall
        // Inside walls
        mergeOrMakeWallFace(tileContainer, prevInnerIndex, wallData, groundZPosition, walls, wallCartesian, CARTESIAN_OPPOSITES[wallCartesian], x, y, z, 0.0f, isNorth * (1.0f - 2.0f * WALL_THICKNESS) + WALL_THICKNESS, true);
        // Outside walls
        if (isNorth) {
            if (y == dims.y - 1 || tileContainer.getWallsMainThread(tileIndex + dims.x).south.wallID == TILE_ID_NONE) {
                mergeOrMakeWallFace(tileContainer, prevOuterIndex, wallData, groundZPosition, walls, wallCartesian, Cartesian(wallCartesian), x, y, z, 0.0f, 1.0f + OUTSIDE_EPSILON, false);
            }
        }
        else {
            if (y == 0 || tileContainer.getWallsMainThread(tileIndex - dims.x).north.wallID == TILE_ID_NONE) {
                mergeOrMakeWallFace(tileContainer, prevOuterIndex, wallData, groundZPosition, walls, wallCartesian, Cartesian(wallCartesian), x, y, z, 0.0f, -OUTSIDE_EPSILON, false);
            }
        }
        
    }
    else {
        // End both prev walls
        if (prevInnerIndex != UINT32_MAX) {
            // End previous wall
            WallChainData& prevWall = wallData[prevInnerIndex];
            prevWall.cornerTypeEnd = WallCornerType::FLAT;
            prevInnerIndex = UINT32_MAX;
        }
        if (prevOuterIndex != UINT32_MAX) {
            // End previous wall
            WallChainData& prevWall = wallData[prevOuterIndex];
            prevWall.cornerTypeEnd = WallCornerType::FLAT;
            prevOuterIndex = UINT32_MAX;
        }
    }
}

void mergeOrMakeWestEastWall(const TileContainer& tileContainer, ui32 x, ui32 y, ui32 z, PrevWallIndices& prevSouthNorthWallIndices, std::vector<WallChainData>& wallData, int isEast) {
    const ui32v3& dims = tileContainer.getDims();
    TileIndex tileIndex = tileContainer.getTileIndexFromXYZOffset(x, y, z);
    const Tile& tile = tileContainer.getTileAt(tileIndex);
    const f32 groundZPosition = tile.getGroundZPositionUncompressedMainThread();
    // TODO: Greedy meshing
    const TileWalls& walls = tileContainer.getWallsMainThread(tileIndex);
    // TODO: Use paint ID
    ui32& prevInnerIndex = prevSouthNorthWallIndices.indices[isEast * 2];
    ui32& prevOuterIndex = prevSouthNorthWallIndices.indices[isEast * 2 + 1];
    const ui32 wallCartesian = 1 + isEast; // Match cartesian
    if (walls.walls[wallCartesian].wallID != TILE_ID_NONE) {
        // We have a wall
        // Inside walls
        mergeOrMakeWallFace(tileContainer, prevInnerIndex, wallData, groundZPosition, walls, wallCartesian, CARTESIAN_OPPOSITES[wallCartesian], x, y, z, isEast * (1.0f - 2.0f * WALL_THICKNESS) + WALL_THICKNESS, 0.0f, true);
        // Outside walls
        if (isEast) {
            if (x == dims.x - 1 || tileContainer.getWallsMainThread(tileIndex + 1).west.wallID == TILE_ID_NONE) {
                mergeOrMakeWallFace(tileContainer, prevOuterIndex, wallData, groundZPosition, walls, wallCartesian, Cartesian(wallCartesian), x, y, z, 1.0f + OUTSIDE_EPSILON, 0.0f, false);
            }
        }
        else {
            if (x == 0 || tileContainer.getWallsMainThread(tileIndex + 1).east.wallID == TILE_ID_NONE) {
                mergeOrMakeWallFace(tileContainer, prevOuterIndex, wallData, groundZPosition, walls, wallCartesian, Cartesian(wallCartesian), x, y, z, -OUTSIDE_EPSILON, 0.0f, false);
            }
        }

    }
    else {
        // End both prev walls
        if (prevInnerIndex != UINT32_MAX) {
            // End previous wall
            WallChainData& prevWall = wallData[prevInnerIndex];
            prevWall.cornerTypeEnd = WallCornerType::FLAT;
            prevInnerIndex = UINT32_MAX;
        }
        if (prevOuterIndex != UINT32_MAX) {
            // End previous wall
            WallChainData& prevWall = wallData[prevOuterIndex];
            prevWall.cornerTypeEnd = WallCornerType::FLAT;
            prevOuterIndex = UINT32_MAX;
        }
    }
}

void meshWalls(const TileContainer& tileContainer, MeshBuilder& meshBuilder, OPT StaticPhysicsMesh* physMesh) {
    // =============== Greedy mesh walls ===============
    // TileID prevSouthWall; Pull ahead greedy meshing like in SoA

    // TODO: separate vector per wall dir? hmm
    const ui32v3& tileDims = tileContainer.getDims();
    assert(tileDims.x <= 256);

    std::vector<WallChainData> wallData;
    wallData.reserve(tileDims.x * tileDims.y);


    for (ui32 z = 0; z < tileDims.z; ++z) {

        PrevWallIndices prevWestEastIndices[257];
        // Construct all wall data by iterating one direction at a time
        // South and North walls (+x)
        for (ui32 y = 0; y < tileDims.y; ++y) {
            // Each row we can merge
            PrevWallIndices prevSouthNorthWallIndices;
            for (ui32 x = 0; x < tileDims.x; ++x) {
                // TODO: We can unify these two methods
                // South
                mergeOrMakeSouthNorthWall(tileContainer, x, y, z, prevSouthNorthWallIndices, wallData, false);
                // North
                mergeOrMakeSouthNorthWall(tileContainer, x, y, z, prevSouthNorthWallIndices, wallData, true);
                // West
                mergeOrMakeWestEastWall(tileContainer, x, y, z, prevWestEastIndices[x], wallData, false);
                // East
                mergeOrMakeWestEastWall(tileContainer, x, y, z, prevWestEastIndices[x], wallData, true);
            }
        }
        // Mesh walls and doors
        f32v3 wallPoints[4];
        f32v3 encapPointsStart[4];
        f32v3 encapPointsEnd[4];
        for (auto&& wall : wallData) {

            // Mesh walls
            bool endcapStart = false;
            bool endcapEnd = false;
            switch (wall.dir) {
                case Cartesian::SOUTH: {
                    wallPoints[0] = wall.startPos;
                    wallPoints[1] = wall.startPos + f32v3(wall.length, 0.0f, 0.0f);
                    wallPoints[2] = wall.startPos + f32v3(wall.length, 0.0f, tileContainer.getFloorHeight());
                    wallPoints[3] = wall.startPos + f32v3(0.0f, 0.0f, tileContainer.getFloorHeight());
                    // Endcaps 
                    // TODO: Endcaps im pretty sure can use lookup array. Maybe walldirs too..
                    if (wall.isPrimary) {
                        if (wall.cornerTypeStart != WallCornerType::NONE) {
                            encapPointsStart[0] = wallPoints[0];
                            encapPointsStart[1] = wallPoints[3];
                            encapPointsStart[2] = wallPoints[3];
                            encapPointsStart[2].y += WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsStart[3] = wallPoints[0];
                            encapPointsStart[3].y += WALL_THICKNESS_PLUS_EPSILON;
                            endcapStart = true;
                        }
                        if (wall.cornerTypeEnd != WallCornerType::NONE) {
                            encapPointsEnd[0] = wallPoints[2];
                            encapPointsEnd[1] = wallPoints[1];
                            encapPointsEnd[2] = wallPoints[1];
                            encapPointsEnd[2].y += WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsEnd[3] = wallPoints[2];
                            encapPointsEnd[3].y += WALL_THICKNESS_PLUS_EPSILON;
                            endcapEnd = true;
                        }
                    }
                    break;
                }
                case Cartesian::WEST: {
                    wallPoints[0] = wall.startPos + f32v3(0.0f, wall.length, 0.0f);
                    wallPoints[1] = wall.startPos;
                    wallPoints[2] = wall.startPos + f32v3(0.0f, 0.0f, tileContainer.getFloorHeight());
                    wallPoints[3] = wall.startPos + f32v3(0.0f, wall.length, tileContainer.getFloorHeight());
                    if (wall.isPrimary) {
                        if (wall.cornerTypeStart != WallCornerType::NONE) {
                            encapPointsStart[0] = wallPoints[0];
                            encapPointsStart[1] = wallPoints[3];
                            encapPointsStart[2] = wallPoints[3];
                            encapPointsStart[2].x += WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsStart[3] = wallPoints[0];
                            encapPointsStart[3].x += WALL_THICKNESS_PLUS_EPSILON;
                            endcapStart = true;
                        }
                        if (wall.cornerTypeEnd != WallCornerType::NONE) {
                            encapPointsEnd[0] = wallPoints[2];
                            encapPointsEnd[1] = wallPoints[1];
                            encapPointsEnd[2] = wallPoints[1];
                            encapPointsEnd[2].x += WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsEnd[3] = wallPoints[2];
                            encapPointsEnd[3].x += WALL_THICKNESS_PLUS_EPSILON;
                            endcapEnd = true;
                        }
                    }
                    break;
                }
                case Cartesian::EAST: {
                    wallPoints[0] = wall.startPos;
                    wallPoints[1] = wall.startPos + f32v3(0.0f, wall.length, 0.0f);
                    wallPoints[2] = wall.startPos + f32v3(0.0f, wall.length, tileContainer.getFloorHeight());
                    wallPoints[3] = wall.startPos + f32v3(0.0f, 0.0f, tileContainer.getFloorHeight());
                    if (wall.isPrimary) {
                        if (wall.cornerTypeStart != WallCornerType::NONE) {
                            encapPointsStart[0] = wallPoints[0];
                            encapPointsStart[1] = wallPoints[3];
                            encapPointsStart[2] = wallPoints[3];
                            encapPointsStart[2].x -= WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsStart[3] = wallPoints[0];
                            encapPointsStart[3].x -= WALL_THICKNESS_PLUS_EPSILON;
                            endcapStart = true;
                        }
                        if (wall.cornerTypeEnd != WallCornerType::NONE) {
                            encapPointsEnd[0] = wallPoints[2];
                            encapPointsEnd[1] = wallPoints[1];
                            encapPointsEnd[2] = wallPoints[1];
                            encapPointsEnd[2].x -= WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsEnd[3] = wallPoints[2];
                            encapPointsEnd[3].x -= WALL_THICKNESS_PLUS_EPSILON;
                            endcapEnd = true;
                        }
                    }
                    break;
                }
                case Cartesian::NORTH: {
                    wallPoints[0] = wall.startPos + f32v3(wall.length, 0.0f, 0.0f);
                    wallPoints[1] = wall.startPos;
                    wallPoints[2] = wall.startPos + f32v3(0.0f, 0.0f, tileContainer.getFloorHeight());
                    wallPoints[3] = wall.startPos + f32v3(wall.length, 0.0f, tileContainer.getFloorHeight());
                    if (wall.isPrimary) {
                        if (wall.cornerTypeStart != WallCornerType::NONE) {
                            encapPointsStart[0] = wallPoints[2];
                            encapPointsStart[1] = wallPoints[1];
                            encapPointsStart[2] = wallPoints[1];
                            encapPointsStart[2].y -= WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsStart[3] = wallPoints[2];
                            encapPointsStart[3].y -= WALL_THICKNESS_PLUS_EPSILON;
                            endcapStart = true;
                        }
                        if (wall.cornerTypeEnd != WallCornerType::NONE) {
                            encapPointsEnd[0] = wallPoints[0];
                            encapPointsEnd[1] = wallPoints[3];
                            encapPointsEnd[2] = wallPoints[3];
                            encapPointsEnd[2].y -= WALL_THICKNESS_PLUS_EPSILON;
                            encapPointsEnd[3] = wallPoints[0];
                            encapPointsEnd[3].y -= WALL_THICKNESS_PLUS_EPSILON;
                            endcapEnd = true;
                        }
                    }
                    break;
                }
                default:
                    assert(false);
                    break;

            }
            // Main faces
            const TileData& tileData = TileRepository::getTileData(wall.tileId);
            const SubTexture& texture = tileData.texture;
            meshBuilder.addQuadBetweenPoints(wallPoints, texture, f32v2(1.0f, -1.0f / 3.0f), COLOR_WHITE);
            if (physMesh) {
                physMesh->addQuadBetweenPoints(wallPoints);
            }
            if (endcapStart) {
                meshBuilder.addQuadBetweenPoints(encapPointsStart, texture, f32v2(1.0f, -1.0f / 3.0f), COLOR_WHITE);
                // The collision is thin enough here we just dont really need it
                /*if (physMesh) {
                    physMesh->addQuadBetweenPoints(encapPointsStart);
                }*/
            }
            if (endcapEnd) {
                meshBuilder.addQuadBetweenPoints(encapPointsEnd, texture, f32v2(1.0f, -1.0f / 3.0f), COLOR_WHITE);
                // The collision is thin enough here we just dont really need it
                /*if (physMesh) {
                    physMesh->addQuadBetweenPoints(encapPointsStart);
                }*/
            }
            
        }
        wallData.clear();
    }
}

void TileMeshBuilderMethods::meshTileContainerStatic(MeshBuilder& meshBuilder, const TileContainer& tileContainer, OPT StaticPhysicsMesh* physMesh) {
    const ui32v3& tileDims = tileContainer.getDims();
    // =============== Mesh tiles ===============
    TileIndex index = 0;
    for (ui32 z = 0; z < tileDims.z; ++z) {
        for (ui32 y = 0; y < tileDims.y; ++y) {
            for (ui32 x = 0; x < tileDims.x; ++x, ++index) {
                const Tile& tile = tileContainer.getTileAt(index);
                const f32 groundZPosition = tile.getGroundZPositionUncompressedMainThread(); // TODO: Thread safe when async
                for (int layerIndex = 0; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.getLayersMainThread()[layerIndex];  // TODO: Thread safe when async
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }

                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    const SubTexture& texture = tileData.texture;

                    // Tile mesh
                    // Flora mesh ONLY
                    if (tileData.shape == TileShape::THIN) {
                        assert(false); // Unsupported
                    }
                    else if (tileData.shape == TileShape::BLOCK) {
                        TileMeshBuilderMethods::addBlock(meshBuilder, f32v3(x, y, z * tileContainer.getFloorHeight()), TileHandle(&tileContainer, index), tileData, physMesh);
                    }
                    else if (tileData.shape == TileShape::FLOOR) {
                        TileMeshBuilderMethods::addFloor(meshBuilder, z * tileContainer.getFloorHeight(), f32v2(x, y), TileHandle(&tileContainer, index), tileData, physMesh);
                    }
                    else if (tileData.shape == TileShape::STAIRS) {
                        TileMeshBuilderMethods::addStairs(meshBuilder, z * tileContainer.getFloorHeight(), f32v2(x, y), TileHandle(&tileContainer, index), tileData, physMesh);
                    }
                }
            }
        }
    }

    meshWalls(tileContainer, meshBuilder, physMesh);
}

constexpr f32 DOOR_THICKNESS = 0.1f;
void TileMeshBuilderMethods::meshTileContainerDynamic(MeshBuilder& meshBuilder, const TileContainer& tileContainer) {
    for (auto&& dynamicTile : tileContainer.getDynamicTiles()) {
        TileIndex tileIndex = dynamicTile.mTileIndex;
        // If this is a wall
        if (dynamicTile.mType <= DynamicTileType::WALL_TERM) {
            Cartesian dir = Cartesian(dynamicTile.mType);
            static_assert(e_cast(DynamicTileType::WALL_SOUTH) == 0 && e_cast(DynamicTileType::WALL_TERM) == 3);
            const TileWall& wall = tileContainer.getWallsMainThread(tileIndex).walls[e_cast(dir)];
            assert(wall.wallID != TILE_ID_NONE);
            // Check if is door
            const TileData& tileData = TileRepository::getTileData(wall.wallID);
            if (tileData.shape == TileShape::DOOR) {
                f32v2 dims;
                f32v3 p1 = tileContainer.getTileXYZOffset(tileIndex);
                p1.z *= tileContainer.getFloorHeight();
                switch (dir) {
                    case Cartesian::SOUTH:
                        dims = f32v2(DOOR_THICKNESS, 0.5f);
                        p1.x += 0.5f;
                        break;
                    case Cartesian::WEST:
                        dims = f32v2(0.5f, DOOR_THICKNESS);
                        p1.y += 0.5f;
                        break;
                    case Cartesian::EAST:
                        dims = f32v2(0.5f, DOOR_THICKNESS);
                        p1.x += 1.0f;
                        p1.y += 0.5f;
                        break;
                    case Cartesian::NORTH:
                        dims = f32v2(DOOR_THICKNESS, 0.5f);
                        p1.x += 0.5f;
                        p1.y += 1.0f;
                        break;
                    default:
                        assert(false);
                        break;

                }
                f32v3 p2 = p1;
                p2.z += tileContainer.getFloorHeight();
                meshBuilder.addBoardBetweenPoints(p1, p2, dims, tileData.texture, f32v2(1.0f, 1.0f / 3.0f));
            }
            else {
                assert(false);
            }
        }
        else {
            assert(false); // Implement other types
        }
    }
}

void TileMeshBuilderMethods::addBlock(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMesh* physMesh) {
    const SubTexture& texture = tileData.texture;
    switch (tileData.textureMethod) {
        case TileTextureMethod::SIMPLE: {
            meshBuilder.addAxisAlignedQuad(
                tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::BOTTOM)],
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
            addBlockVertical(meshBuilder, tilePos, tileHandle, tileData, physMesh);
            break;
        }
        case TileTextureMethod::WORLD_TILING: {
            // todo: FIX
            //int xOff = (ui32)tilePos.x % 8;
            //int yOff = 7 - (ui32)tilePos.y % 8;
            addBlockWorldTiling(meshBuilder, tilePos, tileHandle, tileData, physMesh);
            break;
        }
        case TileTextureMethod::FLORA: {
            // We do not mesh flora in this pass
            break;
        }
    }
    static_assert((int)TileTextureMethod::COUNT == 6, "Implement geo generation for new method");
}

void TileMeshBuilderMethods::addBlockVertical(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMesh* physMesh) {

    const SubTexture& texture = tileData.texture;
    const Tile& tile = *tileHandle.tile;

    const f32 topZPosition = tile.getGroundZPositionUncompressedThreadSafe();
    const f32v3 topPos(tilePos.x, tilePos.y, topZPosition);

    /*TileHandle neighbors[4];
    chunk.getTileNeighbors4(tileHandle.index, neighbors);*/

    // Get height offsets to adjacent tiles
    //const f32 zPosition = tile.groundZPosition; // Dont check terrain here, assume above // TODO: make sure this is right
    const f32 quadHeight = topZPosition - tilePos.z;
    f32 heightDiffs[4];
    // f32 occluderHeight
    // TODO: Fix this
    for (int i = 0; i < 4; ++i) {
        heightDiffs[i] = quadHeight;
    }
    // Old culling
    /*heightDiffs[(int)NeighborIndex4::BOTTOM] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::BOTTOM]);
    heightDiffs[(int)NeighborIndex4::LEFT] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::LEFT]);
    heightDiffs[(int)NeighborIndex4::RIGHT] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::RIGHT]);
    heightDiffs[(int)NeighborIndex4::TOP] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::TOP]);*/

    // Render top
    meshBuilder.addAxisAlignedQuad(
        topPos,
        f32v2(1.0f), // Dimensions
        CubeFacing::TOP,
        texture,
        f32v4(0.0f, 2.0f / 3.0f, 1.0f, 1.0f / 3.0f),
        COLOR_WHITE
    );
    if (physMesh) {
        physMesh->addTileQuad(topPos, f32v2(1.0f), CubeFacing::TOP);

         for (int c = 0; c < 4; ++c) {
             if (heightDiffs[c] > 0.0f) {
                 CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
                 const f32v3 quadPos = tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
                 physMesh->addTileQuad(f32v3(quadPos.x, quadPos.y, quadPos.z), f32v2(1.0f, heightDiffs[c]), quadFacing);
             }
         }
    }

    // Render sides
    const f32v3 topBase = topPos - f32v3(0.0f, 0.0f, 1.0f);
    for (int c = 0; c < 4; ++c) {
        // Render exposed cardinal wall if needed
        if (heightDiffs[c] > 0.0f) {
            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            const f32v3 quadPos = topBase + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
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
                    val = 0;
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

void TileMeshBuilderMethods::addBlockWorldTiling(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMesh* physMesh) {
    const f32 topHeight = tilePos.z + tileHandle.tile->getGroundZPositionUncompressedThreadSafe();

    const f32v3 botSW(tilePos);
    const f32v3 botSE(tilePos.x + 1.0f, tilePos.y, tilePos.z);
    const f32v3 botNW(tilePos.x, tilePos.y + 1.0f, tilePos.z);
    const f32v3 botNE(tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z);
    const f32v3 topSW(tilePos.x, tilePos.y, topHeight);
    const f32v3 topSE(tilePos.x + 1.0f, tilePos.y, topHeight);
    const f32v3 topNW(tilePos.x, tilePos.y + 1.0f, topHeight);
    const f32v3 topNE(tilePos.x + 1.0f, tilePos.y + 1.0f, topHeight);
    meshBuilder.addQuadBetweenPointsWorldUV(topSW, topSE, topNE, topNW, tileData.texture, f32v2(1.0f), COLOR_WHITE, AXIS_Z, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botSW, topSW, topNW, botNW, tileData.texture, f32v2(1.0f), COLOR_WHITE, AXIS_X, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botNE, topNE, topSE, botSE, tileData.texture, f32v2(1.0f), COLOR_WHITE, AXIS_X, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botSW, botSE, topSE, topSW, tileData.texture, f32v2(1.0f), COLOR_WHITE, AXIS_Y, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botNW, topNW, topNE, botNE, tileData.texture, f32v2(1.0f), COLOR_WHITE, AXIS_Y, tilePos, false);
    if (physMesh) {
        physMesh->addQuadBetweenPoints(topSW, topSE, topNE, topNW);
        physMesh->addQuadBetweenPoints(botSW, topSW, topNW, botNW);
        physMesh->addQuadBetweenPoints(botNE, topNE, topSE, botSE);
        physMesh->addQuadBetweenPoints(botSW, botSE, topSE, topSW);
        physMesh->addQuadBetweenPoints(botNW, topNW, topNE, botNE);
    }
}

void TileMeshBuilderMethods::addFloor(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMesh* physMesh) {
    const SubTexture& texture = tileData.texture;
    const f32v3 tilePos(tileXY.x, tileXY.y, floorBaseHeight + 0.0001f);
    // Render top
    meshBuilder.addAxisAlignedQuad(
        tilePos,
        f32v2(1.0f), // Dimensions
        CubeFacing::TOP,
        texture,
        f32v4(0.0f, 2.0f / 3.0f, 1.0f, 1.0f / 3.0f),
        COLOR_WHITE
    );
    if (physMesh) {
        physMesh->addTileQuad(tilePos, f32v2(1.0f), CubeFacing::TOP);
    }
    // TODO: Thickness

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

const i32v2 STAIR_DIR_OFFSETS[CARTESIAN_COUNT] = {
    i32v2(0, 1), // SOUTH
    i32v2(1, 0), // WEST
    i32v2(0, 0), // EAST
    i32v2(0, 0), // NORTH
};

const f32v2 STAIR_DIR_DIMS[CARTESIAN_COUNT] = {
    f32v2(1, 0.25), // SOUTH
    f32v2(0.25, 1), // WEST
    f32v2(0.25, 1), // EAST
    f32v2(1, 0.25), // NORTH
};

void TileMeshBuilderMethods::addStairs(MeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const TileHandle& tileHandle, const TileData& tileData, OPT StaticPhysicsMesh* physMesh)
{
    const TileContainer& tileContainer = *tileHandle.container;
    const Tile& tile = *tileHandle.tile;
    const f32v3 tilePos = tileContainer.getTileXYZOffsetWithZScale(tileHandle.tileIndex);
    // Place stair steps
    // TODO: ThreadSafe
    const f32 heightAdd = tileHandle.tile->getGroundZPositionUncompressedMainThread();
    const Cartesian dir = tile.getOrientationMainThread((TileLayer)tileData.layer);
    const f32v2 stepDir = CARTESIAN_NORMALS[e_cast(dir)];
    constexpr f32 stepWidth = 1.0f / STEPS_PER_TILE;
    const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
    const f32 stairPieceTopHeight = stairPieceBaseHeight + STEPS_PER_TILE * stepHeight;
    AXIS_3D sideUvOrient; // For UV mapping
    AXIS_3D frontUvOrient; // For UV mapping

    //if (stairPiece.isFlatPart) {
    //    // Flat parts are just a single quad on top
    //    const f32v3 pointsTop[4] = {
    //        f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight),
    //        f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight),
    //        f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight),
    //        f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight),
    //    };
    //    meshBuilder.addQuadBetweenPointsWorldUV(pointsTop, rawWoodTexture, f32v2(1.0f), COLOR_WHITE, AXIS_Z, f32v3(0.0f));
    //    building.mPhysicsMesh.addQuadBetweenPoints(pointsTop);
    //    switch (dir) {
    //        case Cartesian::SOUTH:
    //            sideUvOrient = AXIS_X;
    //            break;
    //        case Cartesian::WEST:
    //            sideUvOrient = AXIS_Y;
    //            break;
    //        case Cartesian::EAST:
    //            sideUvOrient = AXIS_Y;
    //            break;
    //        case Cartesian::NORTH:
    //            sideUvOrient = AXIS_X;
    //            break;
    //        default:
    //            assert(false);
    //            break;
    //    }
    //}
    //else {
        // Mesh each step and its sides
    for (i32 step = 0; step < STEPS_PER_TILE; ++step) {
        const f32 height = stairPieceBaseHeight + (step + 1) * stepHeight + 0.0001f/*epsilon*/;
        // Top bit
        const f32v2 stepStride = stepDir * stepWidth;
        const f32v2 stepOffset = (f32)step * stepStride;
        f32v2 cornerPos2D(tilePos.x + stepOffset.x, tilePos.y + stepOffset.y);
        cornerPos2D += f32v2(STAIR_DIR_OFFSETS[e_cast(dir)]) - f32v2(STAIR_DIR_OFFSETS[e_cast(dir)]) * 0.25f;

        const f32v2& stairDims = STAIR_DIR_DIMS[e_cast(dir)];
        const f32v3 pointsTop[4] = {
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x + stairDims.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x + stairDims.x, cornerPos2D.y + stairDims.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y + stairDims.y, height),
        };
        // Side bit and ground bit using switch cause I dont feel like figuring out a clever branchless way
        f32v3 pointsFront[4] = {
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
        };
        // 8 Points since we have two sides
        f32v3 pointsSide[8] = {}; // TODO: UNZERO
        //  We have to reduce the base of each next step 
        switch (dir) {
            case Cartesian::SOUTH:
                sideUvOrient = AXIS_X;
                frontUvOrient = AXIS_Y;
                pointsFront[0].y += stepWidth;
                pointsFront[1].y += stepWidth;
                pointsFront[2].y += stepWidth;
                pointsFront[3].y += stepWidth;

                pointsFront[2].x += 1.0f;
                pointsFront[3].x += 1.0f;

                pointsFront[0].z -= stepHeight;
                pointsFront[3].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsTop[0];
                pointsSide[2] = pointsTop[3];
                pointsSide[3] = pointsFront[0];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight);
                pointsSide[5] = pointsFront[3];
                pointsSide[6] = pointsTop[2];
                pointsSide[7] = pointsTop[1];
                break;
            case Cartesian::WEST:
                sideUvOrient = AXIS_Y;
                frontUvOrient = AXIS_X;
                pointsFront[0].x += stepWidth;
                pointsFront[1].x += stepWidth;
                pointsFront[2].x += stepWidth;
                pointsFront[3].x += stepWidth;

                pointsFront[2].y += 1.0f;
                pointsFront[3].y += 1.0f;

                pointsFront[1].z -= stepHeight;
                pointsFront[2].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsFront[1];
                pointsSide[2] = pointsTop[1];
                pointsSide[3] = pointsTop[0];

                pointsSide[4] = f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsTop[3];
                pointsSide[6] = pointsTop[2];
                pointsSide[7] = pointsFront[2];
                break;
            case Cartesian::EAST:
                sideUvOrient = AXIS_Y;
                frontUvOrient = AXIS_X;
                pointsFront[2].y += 1.0f;
                pointsFront[3].y += 1.0f;

                pointsFront[0].z -= stepHeight;
                pointsFront[3].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsTop[1];
                pointsSide[2] = pointsTop[0];
                pointsSide[3] = pointsFront[0];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsFront[3];
                pointsSide[6] = pointsTop[3];
                pointsSide[7] = pointsTop[2];
                break;
            case Cartesian::NORTH:
                sideUvOrient = AXIS_X;
                frontUvOrient = AXIS_Y;
                pointsFront[2].x += 1.0f;
                pointsFront[3].x += 1.0f;

                pointsFront[1].z -= stepHeight;
                pointsFront[2].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[1] = pointsFront[1];
                pointsSide[2] = pointsTop[0];
                pointsSide[3] = pointsTop[3];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsTop[2];
                pointsSide[6] = pointsTop[1];
                pointsSide[7] = pointsFront[2];
                break;
            default:
                assert(false);
                break;
        }
        const f32v2 uvScale = f32v2(1.0f);
        meshBuilder.addQuadBetweenPointsWorldUV(pointsTop, tileData.texture, uvScale, COLOR_WHITE, AXIS_Z, f32v3(0.0f));
        // The very first step in the entire chain shouldn't have base pieces
        meshBuilder.addQuadBetweenPointsWorldUV(pointsFront, tileData.texture, uvScale, COLOR_WHITE, frontUvOrient, f32v3(0.0f));
        meshBuilder.addQuadBetweenPointsWorldUV(pointsSide, tileData.texture, uvScale, COLOR_WHITE, sideUvOrient, f32v3(0.0f));
        meshBuilder.addQuadBetweenPointsWorldUV(&(pointsSide[4]), tileData.texture, uvScale, COLOR_WHITE, sideUvOrient, f32v3(0.0f));

    }
    // Collision for the side and top of a stair
    f32v3 collisionPointsLeft[3];
    f32v3 collisionPointsRight[3];
    f32v3 collisionPointsRamp[4];
    const f32v3 rampBasePos(tilePos.x, tilePos.y, stairPieceBaseHeight);
    const f32v3 rampTopPos(tilePos.x, tilePos.y, stairPieceTopHeight);
    // Winding doesnt matter
    switch (dir) {
        case Cartesian::SOUTH:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos;
            collisionPointsLeft[2] = rampBasePos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        case Cartesian::WEST:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos;
            collisionPointsLeft[2] = rampBasePos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        case Cartesian::EAST:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsLeft[2] = rampBasePos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[0];
            collisionPointsRamp[1] = collisionPointsLeft[1];
            collisionPointsRamp[2] = collisionPointsRight[1];
            collisionPointsRamp[3] = collisionPointsRight[0];
            break;
        case Cartesian::NORTH:
            collisionPointsLeft[0] = rampBasePos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsLeft[1] = rampTopPos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsLeft[2] = rampBasePos;
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        default:
            assert(false);
            break;
    }
    physMesh->addTriangleBetweenPoints(collisionPointsLeft);
    physMesh->addTriangleBetweenPoints(collisionPointsRight);
    physMesh->addQuadBetweenPoints(collisionPointsRamp);

    // Endcap quad
    // TODO: World space mesher util for cartesian quad?
    // // TODO: Cull this?
    //if (stairPiece.isLastPiece) {
    f32v3 pointsEndcap[4];
    AXIS_3D endcapUvOrient;
    switch (dir) {
        case Cartesian::NORTH:
            endcapUvOrient = AXIS_Y;
            pointsEndcap[0] = { tilePos.x, tilePos.y + 1.0f, tilePos.z };
            pointsEndcap[1] = { tilePos.x, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z };
            break;
        case Cartesian::SOUTH:
            endcapUvOrient = AXIS_Y;
            pointsEndcap[0] = { tilePos.x, tilePos.y, tilePos.z };
            pointsEndcap[1] = { tilePos.x + 1.0f, tilePos.y, tilePos.z };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x, tilePos.y, stairPieceTopHeight };
            break;
        case Cartesian::WEST:
            endcapUvOrient = AXIS_X;
            pointsEndcap[0] = { tilePos.x, tilePos.y, tilePos.z };
            pointsEndcap[1] = { tilePos.x, tilePos.y, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x, tilePos.y + 1.0f, tilePos.z };
            break;
        case Cartesian::EAST:
            endcapUvOrient = AXIS_X;
            pointsEndcap[0] = { tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z };
            pointsEndcap[1] = { tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x + 1.0f, tilePos.y, tilePos.z };
            break;
        default:
            break;

    }
    meshBuilder.addQuadBetweenPointsWorldUV(pointsEndcap, tileData.texture, f32v2(1.0f), COLOR_WHITE, endcapUvOrient, f32v3(0.0f));
    physMesh->addQuadBetweenPoints(pointsEndcap);
       // }
    //}
    // Place square walls to the ground
    f32v3 pointsSide[8] = { tilePos, tilePos, tilePos, tilePos, tilePos, tilePos, tilePos, tilePos };
    switch (dir) {
        case Cartesian::NORTH:
        case Cartesian::SOUTH:
            pointsSide[1].z = stairPieceBaseHeight;
            pointsSide[2].z = stairPieceBaseHeight;
            pointsSide[2].y += 1.0f;
            pointsSide[3].y += 1.0f;

            pointsSide[4] += f32v3(1.0f, 1.0f, 0.0f);
            pointsSide[7].x += 1.0f;
            pointsSide[5] = f32v3(pointsSide[4].x, pointsSide[4].y, stairPieceBaseHeight);
            pointsSide[6] = f32v3(pointsSide[7].x, pointsSide[7].y, stairPieceBaseHeight);
            break;
        case Cartesian::WEST:
        case Cartesian::EAST:
            pointsSide[1].x += 1.0f;
            pointsSide[2].x += 1.0f;
            pointsSide[2].z = stairPieceBaseHeight;
            pointsSide[3].z = stairPieceBaseHeight;

            pointsSide[4] += f32v3(1.0f, 1.0f, 0.0f);
            pointsSide[5].y += 1.0f;
            pointsSide[6] = f32v3(pointsSide[5].x, pointsSide[5].y, stairPieceBaseHeight);
            pointsSide[7] = f32v3(pointsSide[4].x, pointsSide[4].y, stairPieceBaseHeight);
            break;
        default:
            break;

    }
    meshBuilder.addQuadBetweenPointsWorldUV(pointsSide, tileData.texture, f32v2(1.0f), COLOR_WHITE, sideUvOrient, f32v3(0.0f));
    physMesh->addQuadBetweenPoints(pointsSide);
    meshBuilder.addQuadBetweenPointsWorldUV(&(pointsSide[4]), tileData.texture, f32v2(1.0f), COLOR_WHITE, sideUvOrient, f32v3(0.0f));
    physMesh->addQuadBetweenPoints(&(pointsSide[4]));
}

void TileMeshBuilderMethods::addWall(MeshBuilder& meshBuilder, const f32v3& tilePos, const TileData& tileData, Cartesian dir, f32 height, OPT StaticPhysicsMesh* physMesh) {
    constexpr f32 WALL_THICKNESS = 0.05f;
    assert(false);
    //f32v3 rootPos = tilePos
    //meshBuilder.addQuadBetweenPointsWorldUV()
    //meshBuilder.addQuadBetweenPoints()
    //meshBuilder.addAxisAlignedQuad(
    //    topPos,
    //    f32v2(1.0f), // Dimensions
    //    CubeFacing::TOP,
    //    texture,
    //    f32v4(0.0f, 2.0f / 3.0f, 1.0f, 1.0f / 3.0f),
    //    COLOR_WHITE
    //);
    //if (physMesh) {
    //    physMesh->addTileQuad(topPos, f32v2(1.0f), CubeFacing::TOP);

    //    for (int c = 0; c < 4; ++c) {
    //        if (heightDiffs[c] > 0.0f) {
    //            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
    //            const f32v3 quadPos = tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
    //            physMesh->addTileQuad(f32v3(quadPos.x, quadPos.y, quadPos.z), f32v2(1.0f, heightDiffs[c]), quadFacing);
    //        }
    //    }
    //}

}

//
//f32 ChunkMesher::getTileHeight(TileFloor floor, const Tile& neighbor, const f32* heightData, TilePosition tilePos) {
//    f32 height = 0.0f;
//    const TileID tileId = neighbor.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
//    if (tileId != TILE_ID_NONE) {
//        //const TileData& tileData = TileRepository::getTileData(tileId);
//        height = neighbor.getGroundZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles do not count
//      /*  if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = neighbor.getGroundZPositionUncompressedThreadSafe();
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
//        height = tile.getGroundZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles appear to be 1 tile lower
//        /*if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = tile.getGroundZPositionUncompressedThreadSafe();
//        }*/
//    }
//    return glm::max(height, mWorldGrid.computeMinHeightAtTile(TilePosition(neighbor.chunk->getChunkID(), neighbor.index)));
//}
