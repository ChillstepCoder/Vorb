#pragma once

class RoomDef;

#include "util/GridEdge.h"
#include "tile/Stairs.h"

struct RoomGateInfo {
    RoomNodeID adjacentRoom;
    ui32 tileIndex;
};

// Represents a continuous wall segment between two specific rooms
struct RoomWallSegment {
    RoomNodeID adjRooms[2] = { INVALID_ROOM_ID, INVALID_ROOM_ID }; // INVALID_ROOM_ID is outside
    GridEdge edgeInfo;
};
typedef ui32 RoomWallSegmentID;
// TODO: Remove MAX_CHILD_ROOMS

struct RoomGenNode {
    std::vector<RoomWallSegmentID> wallSegmentIDs; //TODO: USE
    std::vector<GridEdge> interiorEdges;
    std::vector<TileIndex> edgeWalk; // These tileindex are relative to the floor
    std::vector<TileIndex> tilePositions;
    const RoomDef* roomDef = nullptr;
    RoomDefID roomDefId;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door or stairs
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via door or stairs, max of 4
    RoomGateInfo adjacentRooms[MAX_ADJACENT_ROOMS]; // Like child rooms, connected via door or open wall, but is not necessarily a direct child, also includes exterior doors
    RoomNodeID id = INVALID_ROOM_ID;
    i32AABB2 aabb = { 0 };
    ui32 numEntrances = 0;
    ui16v2 offsetFromZero;
    ui16 size = 0;
    ui16 desiredSize = 0;
    ui16 desiredWidth = 0;
    ui16 floorIndex = 0;
    ui8 numChildren = 0;
    ui8 numAdjacentRooms = 0;
    bool isPrivate = false;
    bool hasStairs = false;
    bool connectedToParentWithStairs = false;

    bool isDead() const { return desiredSize == 0; }
};