#pragma once

struct RoomDef;

#include "util/GridEdge.h"

// Describes initial four walls
enum class RoomBorderOuterDir {
    LEFT,
    TOP,
    RIGHT,
    BOTTOM
};

// Describes a building graph
struct RoomBorder {
    i16v2 startPos;
    i16v2 endPos;
    RoomBorder* startAdjacent;
    RoomBorder* endAdjacent;
    RoomBorderOuterDir outerDir;
    ui8 length;
};

struct RoomGateInfo {
    RoomNodeID adjacentRoom;
    ui32 tileIndex;
};

struct StairPiece {
    TileIndex pos;
    ui16 height;
    bool isFlatPart : 1;
    bool isLastPiece : 1;
    bool isBuilt : 1;
    bool isReserved : 1;
    Cartesian dir;
};

struct RoomNode {
    RoomDefID roomDefId;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door or stairs
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via door or stairs, max of 4
    RoomGateInfo adjacentRooms[MAX_ADJACENT_ROOMS]; // Like child rooms, connected via door or open wall, but is not necessarily a direct child
    RoomNodeID id = INVALID_ROOM_ID;
    RoomBorder borders[4]; // Used to grow AABB, (TODO: just use AABB? hmm)
    std::vector<GridEdge> interiorEdges;
    std::vector<TileIndex> edgeWalk;
    std::vector<StairPiece> stairs;
    ui32AABB2 aabb = { 0 };
    ui16v2 offsetFromZero;
    ui16 size = 0;
    ui16 desiredSize = 0;
    ui16 desiredWidth = 0;
    ui16 floorIndex = 0;
    ui8 numWalls = 0;
    ui8 numChildren = 0;
    ui8 numAdjacentRooms = 0;
    bool isPrivate = false;
    bool hasStairs = false;
    bool connectedToParentWithStairs = false;
    const RoomDef* roomDef = nullptr;
};